/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsRilResult.h"
#include <cstddef>

/* Logging related */
#undef LOG_TAG
#define LOG_TAG "nsRilResult"

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

#define RILRESULT_CID                                                          \
  {                                                                            \
    0x0a8ace7c, 0xa92a, 0x4c52,                                                \
    {                                                                          \
      0x91, 0x83, 0xf4, 0x39, 0xc7, 0xc1, 0xea, 0xe9                           \
    }                                                                          \
  }

/*============================================================================
 *============ Implementation of Class nsISimPortInfo=============
 *============================================================================*/
NS_IMPL_ISUPPORTS(nsSimPortInfo, nsISimPortInfo)
NS_IMETHODIMP
nsSimPortInfo::GetIccId(nsAString& aIccId)
{
  aIccId = mIccId;
  return NS_OK;
};
NS_IMETHODIMP
nsSimPortInfo::GetLogicalSlotId(int32_t* aLogicalSlotId)
{
  *aLogicalSlotId = mLogicalSlotId;
  return NS_OK;
};
NS_IMETHODIMP
nsSimPortInfo::GetPortActive(bool* aPortActive)
{
  *aPortActive = mPortActive;
  return NS_OK;
};
nsSimPortInfo::nsSimPortInfo(nsAString& aIccId,
                             int32_t aLogicalSlotId,
                             bool aPortActive)
{
  mIccId = aIccId;
  mLogicalSlotId = aLogicalSlotId;
  mPortActive = aPortActive;
};
#if ANDROID_VERSION >= 33
nsSimPortInfo::nsSimPortInfo(const SimPortInfoA& aPortInfo)
{
  mIccId = NS_ConvertUTF8toUTF16(aPortInfo.iccId.c_str());
  mLogicalSlotId = aPortInfo.logicalSlotId;
  mPortActive = aPortInfo.portActive;
};
#endif
/*============================================================================
 *============ Implementation of Class nsISimSlotStatus=============
 *============================================================================*/
NS_IMPL_ISUPPORTS(nsSimSlotStatus, nsISimSlotStatus)

NS_IMETHODIMP
nsSimSlotStatus::GetCardState(int32_t* aCardState)
{
  *aCardState = mCardState;
  return NS_OK;
};

NS_IMETHODIMP
nsSimSlotStatus::GetAtr(nsAString& aAtr)
{
  aAtr = mAtr;
  return NS_OK;
};

NS_IMETHODIMP
nsSimSlotStatus::GetEid(nsAString& aEid)
{
  aEid = mEid;
  return NS_OK;
};

NS_IMETHODIMP
nsSimSlotStatus::GetPortInfo(nsTArray<RefPtr<nsISimPortInfo>>& aPortInfo)
{
  return NS_OK;
};

nsSimSlotStatus::nsSimSlotStatus(int32_t aCardState,
                                 nsAString& aAtr,
                                 nsAString& aEid,
                                 nsTArray<RefPtr<nsISimPortInfo>>& aPortInfo)
{
  mCardState = aCardState;
  mAtr = aAtr;
  mEid = aEid;
  mPortInfo = aPortInfo.Clone();
};
#if ANDROID_VERSION >= 33
nsSimSlotStatus::nsSimSlotStatus(const SimSlotStatusA& aStatus)
{
  mCardState = aStatus.cardState;
  mAtr = NS_ConvertUTF8toUTF16(aStatus.atr.c_str());
  mEid = NS_ConvertUTF8toUTF16(aStatus.eid.c_str());
  for (uint32_t i = 0; i < aStatus.portInfo.size(); i++) {
    mPortInfo.AppendElement(new nsSimPortInfo(aStatus.portInfo[i]));
  }
}
#endif
/*============================================================================
 *============ Implementation of Class nsIBarringTypeSpecificInfo
 *===================
 *============================================================================*/
NS_IMPL_ISUPPORTS(nsBarringTypeSpecificInfo, nsIBarringTypeSpecificInfo)
nsBarringTypeSpecificInfo::nsBarringTypeSpecificInfo(int32_t aFactor,
                                                     int32_t aTimeSeconds,
                                                     bool aIsBarred)
{
  factor = aFactor;
  timeSeconds = aTimeSeconds;
  isBarred = aIsBarred;
}

NS_IMETHODIMP
nsBarringTypeSpecificInfo::GetFactor(int32_t* aParm)
{
  *aParm = factor;
  return NS_OK;
}

NS_IMETHODIMP
nsBarringTypeSpecificInfo::GetTimeSeconds(int32_t* aParm)
{
  *aParm = timeSeconds;
  return NS_OK;
}

NS_IMETHODIMP
nsBarringTypeSpecificInfo::GetIsBarred(bool* aParm)
{
  *aParm = isBarred;
  return NS_OK;
}

/*============================================================================
 *============ Implementation of Class nsIBarringInfo ===================
 *============================================================================*/
NS_IMPL_ISUPPORTS(nsBarringInfo, nsIBarringInfo)
#if ANDROID_VERSION >= 33
nsBarringInfo::nsBarringInfo(const BarringInfo_V1_5& aInfo)
{
  if (aInfo.barringType == BarringInfo_V1_5::BarringType::CONDITIONAL) {
    if (aInfo.barringTypeSpecificInfo.getDiscriminator() !=
        BarringInfo_V1_5::BarringTypeSpecificInfo::hidl_discriminator::
          conditional) {
      serviceType = (int32_t)aInfo.serviceType;
      barringType = (int32_t)aInfo.barringType;

      barringTypeSpecificInfo = new nsBarringTypeSpecificInfo(
        aInfo.barringTypeSpecificInfo.conditional().factor,
        aInfo.barringTypeSpecificInfo.conditional().timeSeconds,
        aInfo.barringTypeSpecificInfo.conditional().isBarred);
    } else {
      serviceType = (int32_t)aInfo.serviceType;
      barringType = (int32_t)aInfo.barringType;
      barringTypeSpecificInfo = new nsBarringTypeSpecificInfo(0, 0, false);
    }
  }
}
#endif
nsBarringInfo::nsBarringInfo(int32_t aServiceType,
                             int32_t aBarringType,
                             int32_t aFactor,
                             int32_t aTimeSeconds,
                             bool aIsBarred)
{
  serviceType = aServiceType;
  barringType = aBarringType;
  barringTypeSpecificInfo =
    new nsBarringTypeSpecificInfo(aFactor, aTimeSeconds, aIsBarred);
}

nsBarringInfo::~nsBarringInfo()
{
  DEBUG("~nsBarringInfo()");
}

NS_IMETHODIMP
nsBarringInfo::GetServiceType(int32_t* aType)
{
  *aType = serviceType;
  return NS_OK;
}

NS_IMETHODIMP
nsBarringInfo::GetBarringType(int32_t* aType)
{
  *aType = barringType;
  return NS_OK;
}

NS_IMETHODIMP
nsBarringInfo::GetBarringTypeSpecificInfo(nsIBarringTypeSpecificInfo** aInfo)
{
  RefPtr<nsIBarringTypeSpecificInfo> data(barringTypeSpecificInfo);
  data.forget(aInfo);
  return NS_OK;
}

/*============================================================================
 *============ Implementation of Class nsBarringInfoChanged ===================
 *============================================================================*/
/**
 * nsIBarringInfoChanged implementation
 */
NS_IMPL_ISUPPORTS(nsBarringInfoChanged, nsIBarringInfoChanged)

nsBarringInfoChanged::nsBarringInfoChanged(
  nsCellIdentity* aCellIdentity,
  nsTArray<RefPtr<nsBarringInfo>>& aBarringInfos)
  : mCellIdentity(aCellIdentity)
  , mBarringInfos(aBarringInfos.Clone())
{
  DEBUG("init nsBarringInfoChanged");
}

NS_IMETHODIMP
nsBarringInfoChanged::GetCellIdentity(nsICellIdentity** aCellIdentity)
{
  RefPtr<nsICellIdentity> cellIdentity(mCellIdentity);
  cellIdentity.forget(aCellIdentity);
  return NS_OK;
}

NS_IMETHODIMP
nsBarringInfoChanged::GetBarringInfos(
  nsTArray<RefPtr<nsIBarringInfo>>& aBarringInfos)
{
  for (uint32_t i = 0; i < mBarringInfos.Length(); i++) {
    aBarringInfos.AppendElement(mBarringInfos[i]);
  }
  return NS_OK;
}

/*============================================================================
 *============ Implementation of Class nsQosBandwidth ===================
 *============================================================================*/
NS_IMPL_ISUPPORTS(nsQosBandwidth, nsIQosBandwidth)

nsQosBandwidth::nsQosBandwidth(int32_t aMaxBitrateKbps,
                               int32_t aGuaranteedBitrateKbps)
{
  maxBitrateKbps = aMaxBitrateKbps;
  guaranteedBitrateKbps = aGuaranteedBitrateKbps;
}

#if ANDROID_VERSION >= 33
nsQosBandwidth::nsQosBandwidth(const QosBandwidth_V1_6& halData)
{
  maxBitrateKbps = halData.maxBitrateKbps;
  guaranteedBitrateKbps = halData.guaranteedBitrateKbps;
}

nsQosBandwidth::nsQosBandwidth(const QosBandwidthA& halData)
{
  maxBitrateKbps = halData.maxBitrateKbps;
  guaranteedBitrateKbps = halData.guaranteedBitrateKbps;
}
#endif

NS_IMETHODIMP
nsQosBandwidth::GetMaxBitrateKbps(int32_t* aParam)
{
  *aParam = maxBitrateKbps;
  return NS_OK;
}

NS_IMETHODIMP
nsQosBandwidth::GetGuaranteedBitrateKbps(int32_t* aParam)
{
  *aParam = guaranteedBitrateKbps;
  return NS_OK;
}

/*============================================================================
 *============ Implementation of Class nsEpsQos ===================
 *============================================================================*/
NS_IMPL_ISUPPORTS(nsEpsQos, nsIEpsQos)

nsEpsQos::nsEpsQos(int32_t aQci,
                   RefPtr<nsIQosBandwidth> aDow,
                   RefPtr<nsIQosBandwidth> aUp)
{
  qci = aQci;
  downlink = aDow;
  uplink = aUp;
}
#if ANDROID_VERSION >= 33
nsEpsQos::nsEpsQos(const EpsQos_V1_6& halEpsQos)
{
  qci = (int32_t)halEpsQos.qci;
  downlink = new nsQosBandwidth(halEpsQos.downlink);
  uplink = new nsQosBandwidth(halEpsQos.uplink);
}

nsEpsQos::nsEpsQos(const EpsQosA& halEpsQos)
{
  qci = (int32_t)halEpsQos.qci;
  downlink = new nsQosBandwidth(halEpsQos.downlink);
  uplink = new nsQosBandwidth(halEpsQos.uplink);
}
#endif

NS_IMETHODIMP
nsEpsQos::GetQci(int32_t* aParam)
{
  *aParam = qci;
  return NS_OK;
}

NS_IMETHODIMP
nsEpsQos::GetDownlink(nsIQosBandwidth** aParam)
{
  RefPtr<nsIQosBandwidth> data(downlink);
  data.forget(aParam);
  return NS_OK;
}

NS_IMETHODIMP
nsEpsQos::GetUplink(nsIQosBandwidth** aParam)
{
  RefPtr<nsIQosBandwidth> data(uplink);
  data.forget(aParam);
  return NS_OK;
}

/*============================================================================
 *============ Implementation of Class nsNrQos ===================
 *============================================================================*/
NS_IMPL_ISUPPORTS(nsNrQos, nsINrQos)
nsNrQos::nsNrQos(int32_t aFiveQi,
                 int32_t aQfi,
                 int32_t aAver,
                 RefPtr<nsIQosBandwidth> aDow,
                 RefPtr<nsIQosBandwidth> aUp)
{
  fiveQi = aFiveQi;
  qfi = aQfi;
  averagingWindowMs = aAver;
  downlink = aDow;
  uplink = aUp;
}

#if ANDROID_VERSION >= 33
nsNrQos::nsNrQos(const NrQos_V1_6& halNrQos)
{
  fiveQi = halNrQos.fiveQi;
  qfi = halNrQos.qfi;
  averagingWindowMs = halNrQos.averagingWindowMs;
  downlink = new nsQosBandwidth(halNrQos.downlink);
  uplink = new nsQosBandwidth(halNrQos.uplink);
}

nsNrQos::nsNrQos(const NrQosA& halNrQos)
{
  fiveQi = halNrQos.fiveQi;
  qfi = halNrQos.qfi;
  averagingWindowMs = (int32_t)halNrQos.averagingWindowMs;
  downlink = new nsQosBandwidth(halNrQos.downlink);
  uplink = new nsQosBandwidth(halNrQos.uplink);
}
#endif

NS_IMETHODIMP
nsNrQos::GetFiveQi(int32_t* aParam)
{
  *aParam = fiveQi;
  return NS_OK;
}

NS_IMETHODIMP
nsNrQos::GetAveragingWindowMs(int32_t* aParam)
{
  *aParam = averagingWindowMs;
  return NS_OK;
}

NS_IMETHODIMP
nsNrQos::GetQfi(int32_t* aParam)
{
  *aParam = qfi;
  return NS_OK;
}

NS_IMETHODIMP
nsNrQos::GetUplink(nsIQosBandwidth** aParam)
{
  RefPtr<nsIQosBandwidth> data(uplink);
  data.forget(aParam);
  return NS_OK;
}

NS_IMETHODIMP
nsNrQos::GetDownlink(nsIQosBandwidth** aParam)
{
  RefPtr<nsIQosBandwidth> data(downlink);
  data.forget(aParam);
  return NS_OK;
}

/*============================================================================
 *============ Implementation of Class nsQos ===================
 *============================================================================*/
NS_IMPL_ISUPPORTS(nsQos, nsIQos)
nsQos::nsQos(int32_t aType, RefPtr<nsIEpsQos> aEps, RefPtr<nsINrQos> aNr)
{
  type = aType;
  eps = aEps;
  nr = aNr;
}

#if ANDROID_VERSION >= 33
nsQos::nsQos(const Qos_V1_6& halQos)
{
  switch (halQos.getDiscriminator()) {
    case Qos_V1_6::hidl_discriminator::eps:
      type = (int32_t)Qos_V1_6::hidl_discriminator::eps;
      eps = new nsEpsQos(halQos.eps());
      nr = nullptr;
      break;
    case Qos_V1_6::hidl_discriminator::nr:
      type = (int32_t)Qos_V1_6::hidl_discriminator::nr;
      nr = new nsNrQos(halQos.nr());
      eps = nullptr;
      break;
    default:
      type = -1;
      eps = nullptr;
      nr = nullptr;
      break;
  }
}

nsQos::nsQos(const QosA& halQos)
{
  switch (halQos.getTag()) {
    case QosA::Tag::eps:
      type = (int32_t)QosA::Tag::eps;
      eps = new nsEpsQos(halQos.get<QosA::Tag::eps>());
      nr = nullptr;
      break;
    case QosA::Tag::nr:
      type = (int32_t)QosA::Tag::nr;
      nr = new nsNrQos(halQos.get<QosA::Tag::nr>());
      eps = nullptr;
      break;
    default:
      type = -1;
      eps = nullptr;
      nr = nullptr;
      break;
  }
}
#endif

NS_IMETHODIMP
nsQos::GetType(int32_t* aParam)
{
  *aParam = type;
  return NS_OK;
}

NS_IMETHODIMP
nsQos::GetEps(nsIEpsQos** aParam)
{
  RefPtr<nsIEpsQos> data(eps);
  data.forget(aParam);
  return NS_OK;
}

NS_IMETHODIMP
nsQos::GetNr(nsINrQos** aParam)
{
  RefPtr<nsINrQos> data(nr);
  data.forget(aParam);
  return NS_OK;
}

/*============================================================================
 *============ Implementation of Class nsPortRange ===================
 *============================================================================*/
NS_IMPL_ISUPPORTS(nsPortRange, nsIPortRange)
nsPortRange::nsPortRange(int32_t aStart, int32_t aEnd)
{
  start = aStart;
  end = aEnd;
}

#if ANDROID_VERSION >= 33
nsPortRange::nsPortRange(const PortRange_V1_6& aRange)
{
  start = aRange.start;
  end = aRange.end;
}

nsPortRange::nsPortRange(const PortRangeA& aRange)
{
  start = aRange.start;
  end = aRange.end;
}
#endif

NS_IMETHODIMP
nsPortRange::GetStart(int32_t* aParam)
{
  *aParam = start;
  return NS_OK;
}

NS_IMETHODIMP
nsPortRange::GetEnd(int32_t* aParam)
{
  *aParam = start;
  return NS_OK;
}

/*============================================================================
 *============ Implementation of Class nsQosFilter ===================
 *============================================================================*/
NS_IMPL_ISUPPORTS(nsQosFilter, nsIQosFilter)

nsQosFilter::nsQosFilter(int32_t aDirection,
                         int32_t aPrecedence,
                         int32_t aProtocol,
                         int32_t aTos,
                         int32_t aFlowLabel,
                         int32_t aSpi,
                         nsTArray<nsAString>& aLocalAddr,
                         nsTArray<nsAString>& aRemoteAddr,
                         RefPtr<nsIPortRange> aLocalPort,
                         RefPtr<nsIPortRange> aRemotePort)
{

  direction = aDirection;
  precedence = aPrecedence;
  protocol = aProtocol;
  tos = aTos;
  flowLabel = aFlowLabel;
  spi = aSpi;
  for (uint32_t i = 0; i < aLocalAddr.Length(); i++) {
    localAddresses.AppendElement(aLocalAddr[i]);
  }

  for (uint32_t i = 0; i < aRemoteAddr.Length(); i++) {
    remoteAddresses.AppendElement(aRemoteAddr[i]);
  }

  localPort = aLocalPort;
  remotePort = aRemotePort;
}

#if ANDROID_VERSION >= 33
nsQosFilter::nsQosFilter(const QosFilter_V1_6& aFilter)
{
  direction = (int32_t)aFilter.direction;
  precedence = aFilter.precedence;
  protocol = (int32_t)aFilter.protocol;
  if (aFilter.tos.getDiscriminator() ==
      TypeOfService_V1_6::hidl_discriminator::value) {
    tos = (int32_t)aFilter.tos.value();
  } else {
    tos = -1;
  }
  if (aFilter.flowLabel.getDiscriminator() ==
      Ipv6FlowLabel_V1_6::hidl_discriminator::value) {
    flowLabel = (int32_t)aFilter.flowLabel.value();
  } else {
    flowLabel = -1;
  }

  if (aFilter.spi.getDiscriminator() ==
      IpsecSpi_V1_6::hidl_discriminator::value) {
    spi = (int32_t)aFilter.spi.value();
  } else {
    spi = -1;
  }

  if (aFilter.localPort.getDiscriminator() ==
      MaybePort_V1_6::hidl_discriminator::range) {
    localPort = new nsPortRange(aFilter.localPort.range());
  } else {
    localPort = nullptr;
  }

  if (aFilter.remotePort.getDiscriminator() ==
      MaybePort_V1_6::hidl_discriminator::range) {
    remotePort = new nsPortRange(aFilter.remotePort.range());
  } else {
    remotePort = nullptr;
  }

  for (uint32_t i = 0; i < aFilter.localAddresses.size(); i++) {
    localAddresses.AppendElement(
      NS_ConvertUTF8toUTF16(aFilter.localAddresses[i].c_str()));
  }

  for (uint32_t i = 0; i < aFilter.remoteAddresses.size(); i++) {
    remoteAddresses.AppendElement(
      NS_ConvertUTF8toUTF16(aFilter.remoteAddresses[i].c_str()));
  }
}

nsQosFilter::nsQosFilter(const QosFilterA& aFilter)
{
  direction = (int32_t)aFilter.direction;
  precedence = aFilter.precedence;
  protocol = (int32_t)aFilter.protocol;
  if (aFilter.tos.getTag() == QosFilterTypeOfService::Tag::value) {
    tos = (int32_t)aFilter.tos.get<QosFilterTypeOfService::Tag::value>();
  } else {
    tos = -1;
  }
  if (aFilter.flowLabel.getTag() == QosFilterIpv6FlowLabel::Tag::value) {
    flowLabel =
      (int32_t)aFilter.flowLabel.get<QosFilterIpv6FlowLabel::Tag::value>();
  } else {
    flowLabel = -1;
  }

  if (aFilter.spi.getTag() == QosFilterIpsecSpi::Tag::value) {
    spi = (int32_t)aFilter.spi.get<QosFilterIpsecSpi::Tag::value>();
  } else {
    spi = -1;
  }
  if (aFilter.localPort.has_value()) {
    localPort = new nsPortRange(aFilter.localPort.value());
  } else {
    localPort = nullptr;
  }

  if (aFilter.remotePort.has_value()) {
    remotePort = new nsPortRange(aFilter.remotePort.value());
  } else {
    remotePort = nullptr;
  }

  for (uint32_t i = 0; i < aFilter.localAddresses.size(); i++) {
    localAddresses.AppendElement(
      NS_ConvertUTF8toUTF16(aFilter.localAddresses[i].c_str()));
  }

  for (uint32_t i = 0; i < aFilter.remoteAddresses.size(); i++) {
    remoteAddresses.AppendElement(
      NS_ConvertUTF8toUTF16(aFilter.remoteAddresses[i].c_str()));
  }
}
#endif

NS_IMETHODIMP
nsQosFilter::GetDirection(int32_t* aParam)
{
  *aParam = direction;
  return NS_OK;
}

NS_IMETHODIMP
nsQosFilter::GetPrecedence(int32_t* aParam)
{
  *aParam = precedence;
  return NS_OK;
}

NS_IMETHODIMP
nsQosFilter::GetProtocol(int32_t* aParam)
{
  *aParam = protocol;
  return NS_OK;
}

NS_IMETHODIMP
nsQosFilter::GetTos(int32_t* aParam)
{
  *aParam = tos;
  return NS_OK;
}

NS_IMETHODIMP
nsQosFilter::GetFlowLabel(int32_t* aParam)
{
  *aParam = flowLabel;
  return NS_OK;
}

NS_IMETHODIMP
nsQosFilter::GetSpi(int32_t* aParam)
{
  *aParam = spi;
  return NS_OK;
}

NS_IMETHODIMP
nsQosFilter::GetLocalAddresses(nsTArray<nsString>& data)
{
  for (uint32_t i = 0; i < localAddresses.Length(); i++) {
    data.AppendElement(localAddresses[i]);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsQosFilter::GetRemoteAddresses(nsTArray<nsString>& data)
{
  for (uint32_t i = 0; i < remoteAddresses.Length(); i++) {
    data.AppendElement(remoteAddresses[i]);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsQosFilter::GetLocalPort(nsIPortRange** aParam)
{
  RefPtr<nsIPortRange> data(localPort);
  data.forget(aParam);
  return NS_OK;
}

NS_IMETHODIMP
nsQosFilter::GetRemotePort(nsIPortRange** aParam)
{
  RefPtr<nsIPortRange> data(remotePort);
  data.forget(aParam);
  return NS_OK;
}

/*============================================================================
 *============ Implementation of Class nsQosSession ===================
 *============================================================================*/
NS_IMPL_ISUPPORTS(nsQosSession, nsIQosSession)

nsQosSession::nsQosSession(int32_t aId,
                           RefPtr<nsIQos> aQos,
                           nsTArray<RefPtr<nsIQosFilter>>& aFilters)
{
  qosSessionId = aId;
  qos = aQos;
  for (uint32_t i = 0; i < aFilters.Length(); i++) {
    qosFilters.AppendElement(aFilters[i]);
  }
}

#if ANDROID_VERSION >= 33
nsQosSession::nsQosSession(const QosSession_V1_6& halData)
{
  qosSessionId = halData.qosSessionId;
  qos = new nsQos(halData.qos);
  for (uint32_t i = 0; i < halData.qosFilters.size(); i++) {
    RefPtr<nsIQosFilter> filter = new nsQosFilter(halData.qosFilters[i]);
    qosFilters.AppendElement(filter);
  }
}

nsQosSession::nsQosSession(const QosSessionA& halData)
{
  qosSessionId = halData.qosSessionId;
  qos = new nsQos(halData.qos);
  for (uint32_t i = 0; i < halData.qosFilters.size(); i++) {
    RefPtr<nsIQosFilter> filter = new nsQosFilter(halData.qosFilters[i]);
    qosFilters.AppendElement(filter);
  }
}
#endif

NS_IMETHODIMP
nsQosSession::GetQosSessionId(int32_t* aParam)
{
  *aParam = qosSessionId;
  return NS_OK;
}

NS_IMETHODIMP
nsQosSession::GetQos(nsIQos** aParam)
{
  RefPtr<nsIQos> data(qos);
  data.forget(aParam);
  return NS_OK;
}

NS_IMETHODIMP
nsQosSession::GetQosFilters(nsTArray<RefPtr<nsIQosFilter>>& aQosFilters)
{
  for (uint32_t i = 0; i < qosFilters.Length(); i++) {
    aQosFilters.AppendElement(qosFilters[i]);
  }
  return NS_OK;
}

/*============================================================================
 *============ Implementation of Class nsGetBarringInfoResult
 *===================
 *============================================================================*/
NS_IMPL_ISUPPORTS(nsGetBarringInfoResult, nsIGetBarringInfoResult)

nsGetBarringInfoResult::nsGetBarringInfoResult(
  RefPtr<nsICellIdentity> aCellIdentity,
  nsTArray<RefPtr<nsIBarringInfo>>& aBarringInfoList)
{
  mCellIdentity = aCellIdentity;
  for (uint32_t i = 0; i < aBarringInfoList.Length(); i++) {
    mBarringInfos.AppendElement(aBarringInfoList[i]);
  }
}

NS_IMETHODIMP
nsGetBarringInfoResult::GetCellIdentity(nsICellIdentity** aCellIdentity)
{
  RefPtr<nsICellIdentity> data(mCellIdentity);
  data.forget(aCellIdentity);
  return NS_OK;
}

NS_IMETHODIMP
nsGetBarringInfoResult::GetBarringInfos(
  nsTArray<RefPtr<nsIBarringInfo>>& aBarringInfos)
{
  for (uint32_t i = 0; i < aBarringInfos.Length(); i++) {
    aBarringInfos.AppendElement(mBarringInfos[i]);
  }
  return NS_OK;
}

/*============================================================================
 *============ Implementation of Class nsSliceInfo ===================
 *============================================================================*/
/**
 * nsITrafficDescriptor implementation
 */
NS_IMPL_ISUPPORTS(nsSliceInfo, nsISliceInfo)

nsSliceInfo::nsSliceInfo(int32_t aSst,
                         int32_t aSliceDifferentiator,
                         int32_t aMappedHplmnSst,
                         int32_t aMappedHplmnSD,
                         int32_t aStatus)
{
  sst = aSst;
  sliceDifferentiator = aSliceDifferentiator;
  mappedHplmnSst = aMappedHplmnSst;
  mappedHplmnSD = aMappedHplmnSD;
  status = aStatus;
}

#if ANDROID_VERSION >= 33
nsSliceInfo::nsSliceInfo(const SliceInfo_V1_6& aSliceInfo)
{
  sst = (int32_t)aSliceInfo.sst;
  sliceDifferentiator = (int32_t)aSliceInfo.sliceDifferentiator;
  mappedHplmnSst = (int32_t)aSliceInfo.mappedHplmnSst;
  mappedHplmnSD = (int32_t)aSliceInfo.mappedHplmnSD;
  status = (int32_t)aSliceInfo.status;
}

nsSliceInfo::nsSliceInfo(const SliceInfoA& aSliceInfo)
{
  sst = (int32_t)aSliceInfo.sliceServiceType;
  sliceDifferentiator = (int32_t)aSliceInfo.sliceDifferentiator;
  mappedHplmnSst = (int32_t)aSliceInfo.mappedHplmnSst;
  mappedHplmnSD = (int32_t)aSliceInfo.mappedHplmnSd;
  status = (int32_t)aSliceInfo.status;
}
#endif

NS_IMETHODIMP
nsSliceInfo::GetSst(int32_t* aParm)
{
  *aParm = sst;
  return NS_OK;
}

NS_IMETHODIMP
nsSliceInfo::GetSliceDifferentiator(int32_t* aParm)
{
  *aParm = sliceDifferentiator;
  return NS_OK;
}

NS_IMETHODIMP
nsSliceInfo::GetMappedHplmnSst(int32_t* aParm)
{
  *aParm = mappedHplmnSst;
  return NS_OK;
}

NS_IMETHODIMP
nsSliceInfo::GetMappedHplmnSD(int32_t* aParm)
{
  *aParm = mappedHplmnSD;
  return NS_OK;
}

NS_IMETHODIMP
nsSliceInfo::GetStatus(int32_t* aParm)
{
  *aParm = status;
  return NS_OK;
}

/*============================================================================
 *======================Implementation of Class nsLinkAddress
 *=====================
 *============================================================================*/
/**
 * Constructor for a nsLinkAddress
 * For those has no parameter notify.
 */
nsLinkAddress::nsLinkAddress(const nsAString& aAddress,
                             int32_t aProperties,
                             uint64_t aDeprecationTime,
                             uint64_t aExpirationTime)
  : mAddress(aAddress)
  , mProperties(aProperties)
  , mDeprecationTime(aDeprecationTime)
  , mExpirationTime(aExpirationTime)
{
  DEBUG("init nsLinkAddress.");
}
NS_IMETHODIMP
nsLinkAddress::GetAddress(nsAString& aAddress)
{
  aAddress = mAddress;
  return NS_OK;
}
NS_IMETHODIMP
nsLinkAddress::GetProperties(int32_t* aProperties)
{
  *aProperties = mProperties;
  return NS_OK;
}
NS_IMETHODIMP
nsLinkAddress::GetDeprecationTime(uint64_t* aDeprecationTime)
{
  *aDeprecationTime = mDeprecationTime;
  return NS_OK;
}
NS_IMETHODIMP
nsLinkAddress::GetExpirationTime(uint64_t* aExpiryTime)
{
  *aExpiryTime = mExpirationTime;
  return NS_OK;
}
NS_IMPL_ISUPPORTS(nsLinkAddress, nsILinkAddress)

/*============================================================================
 *============ Implementation of Class nsIRouteSelectionDescriptor
 *===================
 *============================================================================*/
/**
 * nsIRouteSelectionDescriptor implementation
 */
NS_IMPL_ISUPPORTS(nsRouteSelectionDescriptor, nsIRouteSelectionDescriptor)

nsRouteSelectionDescriptor::nsRouteSelectionDescriptor(
  int32_t aPrecedence,
  int32_t aSessionType,
  int32_t aSscMode,
  nsTArray<RefPtr<nsISliceInfo>>& aSliceInfo,
  nsTArray<nsString>& aDnn)
{
  precedence = aPrecedence;
  sessionType = aSessionType;
  sscMode = aSscMode;
  for (uint32_t i = 0; i < aSliceInfo.Length(); i++) {
    sliceInfo.AppendElement(aSliceInfo[i]);
  }
  dnn = aDnn.Clone();
}
#if ANDROID_VERSION >= 33
nsRouteSelectionDescriptor::nsRouteSelectionDescriptor(
  RouteSelectionDescriptor_V1_6& aRouteSelectionDescriptor)
{
  precedence = (int32_t)aRouteSelectionDescriptor.precedence;
  if (aRouteSelectionDescriptor.sessionType.getDiscriminator() ==
      OptionalPdpProtocolType_V1_6::hidl_discriminator::value) {
    sessionType = (int32_t)aRouteSelectionDescriptor.sessionType.value();
  } else {
    sessionType = -1;
  }

  if (aRouteSelectionDescriptor.sscMode.getDiscriminator() ==
      OptionalSscMode_V1_6::hidl_discriminator::value) {
    sscMode = (int32_t)aRouteSelectionDescriptor.sscMode.value();
  }

  for (uint32_t i = 0; i < aRouteSelectionDescriptor.sliceInfo.size(); i++) {
    SliceInfo_V1_6 info = aRouteSelectionDescriptor.sliceInfo[i];
    RefPtr<nsISliceInfo> nsInfo = new nsSliceInfo(info);
    sliceInfo.AppendElement(nsInfo);
  }
  for (uint32_t i = 0; i < aRouteSelectionDescriptor.dnn.size(); i++) {
    dnn.AppendElement(
      NS_ConvertUTF8toUTF16(aRouteSelectionDescriptor.dnn[i].c_str()));
  }
}

nsRouteSelectionDescriptor::nsRouteSelectionDescriptor(
  const RouteSelectionDescriptorA& aRouteSelectionDescriptor)
{
  precedence = (int32_t)aRouteSelectionDescriptor.precedence;
  sessionType = (int32_t)aRouteSelectionDescriptor.sessionType;
  sscMode = (int32_t)aRouteSelectionDescriptor.sscMode;

  for (uint32_t i = 0; i < aRouteSelectionDescriptor.sliceInfo.size(); i++) {
    RefPtr<nsISliceInfo> nsInfo =
      new nsSliceInfo(aRouteSelectionDescriptor.sliceInfo[i]);
    sliceInfo.AppendElement(nsInfo);
  }
  for (uint32_t i = 0; i < aRouteSelectionDescriptor.dnn.size(); i++) {
    dnn.AppendElement(
      NS_ConvertUTF8toUTF16(aRouteSelectionDescriptor.dnn[i].c_str()));
  }
}
#endif

NS_IMETHODIMP
nsRouteSelectionDescriptor::GetPrecedence(int32_t* aPrecedence)
{
  *aPrecedence = precedence;
  return NS_OK;
}

NS_IMETHODIMP
nsRouteSelectionDescriptor::GetSessionType(int32_t* aSessionType)
{
  *aSessionType = sessionType;
  return NS_OK;
}

NS_IMETHODIMP
nsRouteSelectionDescriptor::GetSscMode(int32_t* aSscMode)
{
  *aSscMode = sscMode;
  return NS_OK;
}

NS_IMETHODIMP
nsRouteSelectionDescriptor::GetSliceInfo(nsTArray<RefPtr<nsISliceInfo>>& aParm)
{
  for (uint32_t i = 0; i < sliceInfo.Length(); i++) {
    aParm.AppendElement(sliceInfo[i]);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsRouteSelectionDescriptor::GetDnn(nsTArray<nsString>& aParm)
{
  aParm = dnn.Clone();
  return NS_OK;
}

/*============================================================================
 *============ Implementation of Class nsITrafficDescriptor ===================
 *============================================================================*/
/**
 * nsITrafficDescriptor implementation
 */
NS_IMPL_ISUPPORTS(nsTrafficDescriptor, nsITrafficDescriptor)

nsTrafficDescriptor::nsTrafficDescriptor(nsAString& aDnn,
                                         const nsTArray<int32_t>& aOsAppId)
{
  mDnn = aDnn;
  mOsAppId = aOsAppId.Clone();
}
#if ANDROID_VERSION >= 33
nsTrafficDescriptor::nsTrafficDescriptor(const TrafficDescriptor_V1_6& aParm)
{
  OptionalOsAppId_V1_6 osAppId = aParm.osAppId;
  if (osAppId.getDiscriminator() ==
      OptionalOsAppId_V1_6::hidl_discriminator::value) {
    const OsAppId_V1_6 value = osAppId.value();
    for (uint32_t i = 0; i < value.osAppId.size(); i++) {
      mOsAppId.AppendElement((int32_t)value.osAppId[i]);
    }
  }
  OptionalDnn_V1_6 dnn = aParm.dnn;
  if (dnn.getDiscriminator() == OptionalDnn_V1_6::hidl_discriminator::value) {
    ::android::hardware::hidl_string data = dnn.value();
    mDnn = NS_ConvertUTF8toUTF16(data.c_str());
  }
}

nsTrafficDescriptor::nsTrafficDescriptor(const TrafficDescriptorA& aParm)
{
  if (aParm.osAppId.has_value()) {
    for (uint32_t i = 0; i < aParm.osAppId.value().osAppId.size(); i++) {
      mOsAppId.AppendElement(aParm.osAppId.value().osAppId[i]);
    }
  }

  if (aParm.dnn.has_value()) {
    mDnn = NS_ConvertUTF8toUTF16(aParm.dnn.value().c_str());
  }
}
#endif

NS_IMETHODIMP
nsTrafficDescriptor::GetDnn(nsAString& aDnn)
{
  aDnn = mDnn;
  return NS_OK;
}

NS_IMETHODIMP
nsTrafficDescriptor::GetOsAppId(nsTArray<int32_t>& aOsAppId)
{
  aOsAppId = mOsAppId.Clone();
  return NS_OK;
}

/*============================================================================
 *============ Implementation of Class nsRegistrationFailedEvent
 *===================
 *============================================================================*/
/**
 * nsIRegistrationFailedEvent implementation
 */
NS_IMPL_ISUPPORTS(nsRegistrationFailedEvent, nsIRegistrationFailedEvent)

nsRegistrationFailedEvent::nsRegistrationFailedEvent(
  nsCellIdentity* aCellIdentity,
  const nsAString& aChosenPlmn,
  int32_t aDomain,
  int32_t aCauseCode,
  int32_t aAdditionalCauseCode)
  : mCellIdentity(aCellIdentity)
  , mChosenPlmn(aChosenPlmn)
  , mDomain(aDomain)
  , mCauseCode(aCauseCode)
  , mAdditionalCauseCode(aAdditionalCauseCode)
{
  DEBUG("init nsRegistrationFailedEvent");
}

NS_IMETHODIMP
nsRegistrationFailedEvent::GetCellIdentity(nsICellIdentity** aCellIdentity)
{
  RefPtr<nsICellIdentity> cellIdentity(mCellIdentity);
  cellIdentity.forget(aCellIdentity);
  return NS_OK;
}

NS_IMETHODIMP
nsRegistrationFailedEvent::GetChosenPlmn(nsAString& aChosenPlmn)
{
  aChosenPlmn = mChosenPlmn;
  return NS_OK;
}

NS_IMETHODIMP
nsRegistrationFailedEvent::GetDomain(int32_t* aDomain)
{
  *aDomain = mDomain;
  return NS_OK;
}

NS_IMETHODIMP
nsRegistrationFailedEvent::GetCauseCode(int32_t* aCauseCode)
{
  *aCauseCode = mCauseCode;
  return NS_OK;
}

NS_IMETHODIMP
nsRegistrationFailedEvent::GetAdditionalCauseCode(int32_t* aAdditionalCauseCode)
{
  *aAdditionalCauseCode = mAdditionalCauseCode;
  return NS_OK;
}

/*============================================================================
 *============ Implementation of Class nsIUrspRule ===================
 *============================================================================*/
/**
 * nsIUrspRule implementation
 */
NS_IMPL_ISUPPORTS(nsUrspRule, nsIUrspRule)

nsUrspRule::nsUrspRule(
  int32_t aPrecedence,
  nsTArray<RefPtr<nsITrafficDescriptor>>& aTrafficDescriptors,
  nsTArray<RefPtr<nsIRouteSelectionDescriptor>>& aRouteSelectionDescriptor)
{
  precedence = aPrecedence;

  for (uint32_t i = 0; i < aTrafficDescriptors.Length(); i++) {
    trafficDescriptors.AppendElement(aTrafficDescriptors[i]);
  }

  for (uint32_t i = 0; i < aRouteSelectionDescriptor.Length(); i++) {
    routeSelectionDescriptor.AppendElement(aRouteSelectionDescriptor[i]);
  }
}
#if ANDROID_VERSION >= 33
nsUrspRule::nsUrspRule(const UrspRule_V1_6& aRule)
{
  precedence = aRule.precedence;

  for (uint32_t i = 0; i < aRule.trafficDescriptors.size(); i++) {
    TrafficDescriptor_V1_6 aTraffic = aRule.trafficDescriptors[i];
    RefPtr<nsITrafficDescriptor> ptr = new nsTrafficDescriptor(aTraffic);
    trafficDescriptors.AppendElement(ptr);
  }

  for (uint32_t i = 0; i < aRule.routeSelectionDescriptor.size(); i++) {
    RouteSelectionDescriptor_V1_6 rsd = aRule.routeSelectionDescriptor[i];
    RefPtr<nsIRouteSelectionDescriptor> ptr =
      new nsRouteSelectionDescriptor(rsd);
    routeSelectionDescriptor.AppendElement(ptr);
  }
}

nsUrspRule::nsUrspRule(const UrspRuleA& aRule)
{
  precedence = aRule.precedence;

  for (uint32_t i = 0; i < aRule.trafficDescriptors.size(); i++) {
    RefPtr<nsITrafficDescriptor> ptr =
      new nsTrafficDescriptor(aRule.trafficDescriptors[i]);
    trafficDescriptors.AppendElement(ptr);
  }

  for (uint32_t i = 0; i < aRule.routeSelectionDescriptor.size(); i++) {
    RefPtr<nsIRouteSelectionDescriptor> ptr =
      new nsRouteSelectionDescriptor(aRule.routeSelectionDescriptor[i]);
    routeSelectionDescriptor.AppendElement(ptr);
  }
}
#endif

NS_IMETHODIMP
nsUrspRule::GetPrecedence(int32_t* aPrecedence)
{
  *aPrecedence = precedence;
  return NS_OK;
}

NS_IMETHODIMP
nsUrspRule::GetTrafficDescriptors(
  nsTArray<RefPtr<nsITrafficDescriptor>>& aTrafficDescriptors)
{
  for (uint32_t i = 0; i < trafficDescriptors.Length(); i++) {
    aTrafficDescriptors.AppendElement(trafficDescriptors[i]);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsUrspRule::GetRouteSelectionDescriptor(
  nsTArray<RefPtr<nsIRouteSelectionDescriptor>>& aRouteSelectionDescriptor)
{
  for (uint32_t i = 0; i < routeSelectionDescriptor.Length(); i++) {
    aRouteSelectionDescriptor.AppendElement(routeSelectionDescriptor[i]);
  }
  return NS_OK;
}

/*============================================================================
 *============ Implementation of Class nsISlicingConfig ===================
 *============================================================================*/
/**
 * nsISlicingConfig implementation
 */
NS_IMPL_ISUPPORTS(nsSlicingConfig, nsISlicingConfig)

nsSlicingConfig ::nsSlicingConfig(nsTArray<RefPtr<nsIUrspRule>>& aRules,
                                  nsTArray<RefPtr<nsISliceInfo>>& aSliceInfos)
{
  for (unsigned int i = 0; i < aRules.Length(); i++) {
    urspRules.AppendElement(aRules[i]);
  }

  for (unsigned int i = 0; i < aSliceInfos.Length(); i++) {
    sliceInfo.AppendElement(aSliceInfos[i]);
  }
}
#if ANDROID_VERSION >= 33
nsSlicingConfig::nsSlicingConfig(const SlicingConfig_V1_6& aConfig)
{
  for (uint32_t i = 0; i < aConfig.urspRules.size(); i++) {
    urspRules.AppendElement(new nsUrspRule(aConfig.urspRules[i]));
  }

  for (uint32_t i = 0; i < aConfig.sliceInfo.size(); i++) {
    sliceInfo.AppendElement(new nsSliceInfo(aConfig.sliceInfo[i]));
  }
}
#endif
NS_IMETHODIMP
nsSlicingConfig::GetUrspRules(nsTArray<RefPtr<nsIUrspRule>>& aRules)
{
  for (unsigned int i = 0; i < urspRules.Length(); i++) {
    aRules.AppendElement(urspRules[i]);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSlicingConfig::GetSliceInfo(nsTArray<RefPtr<nsISliceInfo>>& aSliceInfo)
{
  for (unsigned int i = 0; i < sliceInfo.Length(); i++) {
    aSliceInfo.AppendElement(sliceInfo[i]);
  }
  return NS_OK;
}

/*============================================================================
 *============ Implementation of Class nsIPhonebookCapacity ===================
 *============================================================================*/
/**
 * nsIPhonebookCapacity implementation
 */
NS_IMPL_ISUPPORTS(nsPhonebookCapacity, nsIPhonebookCapacity)
#if ANDROID_VERSION >= 33
nsPhonebookCapacity::nsPhonebookCapacity(const PhonebookCapacity_V1_6& aParam)
{
  maxAdnRecords = aParam.maxAdnRecords;
  usedAdnRecords = aParam.usedAdnRecords;
  maxEmailRecords = aParam.maxEmailRecords;
  usedEmailRecords = aParam.usedEmailRecords;
  maxAdditionalNumberLen = aParam.maxAdditionalNumberLen;
  maxAdditionalNumberRecords = aParam.maxAdditionalNumberRecords;
  maxNameLen = aParam.maxNameLen;
  maxNumberLen = aParam.maxNumberLen;
  maxEmailLen = aParam.maxEmailLen;
  usedAdditionalNumberRecords = aParam.usedAdditionalNumberRecords;
}
#endif
nsPhonebookCapacity::nsPhonebookCapacity(int32_t aMaxAdnRecords,
                                         int32_t aUsedAdnRecords,
                                         int32_t aMaxEmailRecords,
                                         int32_t aUsedEmailRecords,
                                         int32_t aMaxAdditionalNumberRecords,
                                         int32_t aUsedAdditionalNumberRecords,
                                         int32_t aMaxNameLen,
                                         int32_t aMaxNumberLen,
                                         int32_t aMaxEmailLen,
                                         int32_t aMaxAdditionalNumberLen)
{
  maxAdnRecords = aMaxAdnRecords;
  usedAdnRecords = aUsedAdnRecords;
  maxEmailRecords = aMaxEmailRecords;
  usedEmailRecords = aUsedEmailRecords;
  maxAdditionalNumberRecords = aMaxAdditionalNumberRecords;
  usedAdditionalNumberRecords = aUsedAdditionalNumberRecords;
  maxNameLen = aMaxNameLen;
  maxEmailLen = aMaxEmailLen;
  maxAdditionalNumberLen = aMaxAdditionalNumberLen;
  maxNumberLen = aMaxNumberLen;
}

NS_IMETHODIMP
nsPhonebookCapacity::GetMaxNumberLen(int32_t* aParam)
{
  *aParam = maxNameLen;
  return NS_OK;
}

NS_IMETHODIMP
nsPhonebookCapacity::GetMaxAdditionalNumberRecords(int32_t* aParam)
{
  *aParam = maxAdditionalNumberRecords;
  return NS_OK;
}

NS_IMETHODIMP
nsPhonebookCapacity::GetMaxAdnRecords(int32_t* aParam)
{
  *aParam = maxAdnRecords;
  return NS_OK;
}

NS_IMETHODIMP
nsPhonebookCapacity::GetUsedAdnRecords(int32_t* aParam)
{
  *aParam = usedAdnRecords;
  return NS_OK;
}

NS_IMETHODIMP
nsPhonebookCapacity::GetMaxNameLen(int32_t* aParam)
{
  *aParam = maxNameLen;
  return NS_OK;
}

NS_IMETHODIMP
nsPhonebookCapacity::GetMaxAdditionalNumberLen(int32_t* aParam)
{
  *aParam = maxAdditionalNumberLen;
  return NS_OK;
}

NS_IMETHODIMP
nsPhonebookCapacity::GetMaxEmailLen(int32_t* aParam)
{
  *aParam = maxEmailLen;
  return NS_OK;
}

NS_IMETHODIMP
nsPhonebookCapacity::GetUsedAdditionalNumberRecords(int32_t* aParam)
{
  *aParam = usedAdditionalNumberRecords;
  return NS_OK;
}

NS_IMETHODIMP
nsPhonebookCapacity::GetUsedEmailRecords(int32_t* aParam)
{
  *aParam = usedEmailRecords;
  return NS_OK;
}

NS_IMETHODIMP
nsPhonebookCapacity::GetMaxEmailRecords(int32_t* aParam)
{
  *aParam = maxEmailRecords;
  return NS_OK;
}

/*============================================================================
 *============ Implementation of Class nsSupplySimDepersonalizationResult
 *===================
 *============================================================================*/
/**
 * nsSupplySimDepersonalizationResult implementation
 */
NS_IMPL_ISUPPORTS(nsSupplySimDepersonalizationResult,
                  nsISupplySimDepersonalizationResult)

nsSupplySimDepersonalizationResult::nsSupplySimDepersonalizationResult(
  int32_t aType,
  int32_t aRemainingRetries)
{
  mPersoType = aType;
  mRemainingRetries = aRemainingRetries;
}

NS_IMETHODIMP
nsSupplySimDepersonalizationResult::GetPersoType(int32_t* aPersoType)
{
  *aPersoType = mPersoType;
  return NS_OK;
}

NS_IMETHODIMP
nsSupplySimDepersonalizationResult::GetRemainingRetries(int32_t* aRetries)
{
  *aRetries = mRemainingRetries;
  return NS_OK;
}

/*============================================================================
 *============ Implementation of Class nsKeepAliveStatus ===================
 *============================================================================*/
/**
 * nsKeepAliveStatus implementation
 */
NS_IMPL_ISUPPORTS(nsKeepAliveStatus, nsIKeepAliveStatus)

nsKeepAliveStatus::nsKeepAliveStatus(int32_t aHandle, int32_t aStatus)
{
  mSessionHandle = aHandle;
  mKeepAliveStatusCode = aStatus;
}

NS_IMETHODIMP
nsKeepAliveStatus::GetSessionHandle(int32_t* aHandle)
{
  *aHandle = mSessionHandle;
  return NS_OK;
}

NS_IMETHODIMP
nsKeepAliveStatus::GetKeepAliveStatusCode(int32_t* aCode)
{
  *aCode = mKeepAliveStatusCode;
  return NS_OK;
}

/*============================================================================
 *============ Implementation of Class nsGsmSignalStrength ===================
 *============================================================================*/
/**
 * nsGsmSignalStrength implementation
 */
nsGsmSignalStrength::nsGsmSignalStrength(int32_t aSignalStrength,
                                         int32_t aBitErrorRate,
                                         int32_t aTimingAdvance)
  : mSignalStrength(aSignalStrength)
  , mBitErrorRate(aBitErrorRate)
  , mTimingAdvance(aTimingAdvance)
{
  DEBUG("init nsGsmSignalStrength");
}

NS_IMETHODIMP
nsGsmSignalStrength::GetSignalStrength(int32_t* aSignalStrength)
{
  *aSignalStrength = mSignalStrength;
  return NS_OK;
}
NS_IMETHODIMP
nsGsmSignalStrength::GetBitErrorRate(int32_t* aBitErrorRate)
{
  *aBitErrorRate = mBitErrorRate;
  return NS_OK;
}
NS_IMETHODIMP
nsGsmSignalStrength::GetTimingAdvance(int32_t* aTimingAdvance)
{
  *aTimingAdvance = mTimingAdvance;
  return NS_OK;
}
NS_IMPL_ISUPPORTS(nsGsmSignalStrength, nsIGsmSignalStrength)

/*============================================================================
 *============ Implementation of Class nsWcdmaSignalStrength ===================
 *============================================================================*/
/**
 * nsWcdmaSignalStrength implementation
 */
nsWcdmaSignalStrength::nsWcdmaSignalStrength(int32_t aSignalStrength,
                                             int32_t aBitErrorRate,
                                             int32_t aRscp,
                                             int32_t aEcno)
  : mSignalStrength(aSignalStrength)
  , mBitErrorRate(aBitErrorRate)
  , mRscp(aRscp)
  , mEcno(aEcno)
{
  DEBUG("init nsWcdmaSignalStrength");
}

NS_IMETHODIMP
nsWcdmaSignalStrength::GetSignalStrength(int32_t* aSignalStrength)
{
  *aSignalStrength = mSignalStrength;
  return NS_OK;
}

NS_IMETHODIMP
nsWcdmaSignalStrength::GetBitErrorRate(int32_t* aBitErrorRate)
{
  *aBitErrorRate = mBitErrorRate;
  return NS_OK;
}

NS_IMETHODIMP
nsWcdmaSignalStrength::GetRscp(int32_t* aRscp)
{
  *aRscp = mRscp;
  return NS_OK;
}

NS_IMETHODIMP
nsWcdmaSignalStrength::GetEcno(int32_t* aEcno)
{
  *aEcno = mEcno;
  return NS_OK;
}
NS_IMPL_ISUPPORTS(nsWcdmaSignalStrength, nsIWcdmaSignalStrength)

/*============================================================================
 *============ Implementation of Class nsCdmaSignalStrength ===================
 *============================================================================*/
/**
 * nsCdmaSignalStrength implementation
 */
nsCdmaSignalStrength::nsCdmaSignalStrength(int32_t aDbm, int32_t aEcio)
  : mDbm(aDbm)
  , mEcio(aEcio)
{
  DEBUG("init nsCdmaSignalStrength");
}

NS_IMETHODIMP
nsCdmaSignalStrength::GetDbm(int32_t* aDbm)
{
  *aDbm = mDbm;
  return NS_OK;
}
NS_IMETHODIMP
nsCdmaSignalStrength::GetEcio(int32_t* aEcio)
{
  *aEcio = mEcio;
  return NS_OK;
}
NS_IMPL_ISUPPORTS(nsCdmaSignalStrength, nsICdmaSignalStrength)

/*============================================================================
 *============ Implementation of Class nsEvdoSignalStrength ===================
 *============================================================================*/
/**
 * nsEvdoSignalStrength implementation
 */
nsEvdoSignalStrength::nsEvdoSignalStrength(int32_t aDbm,
                                           int32_t aEcio,
                                           int32_t aSignalNoiseRatio)
  : mDbm(aDbm)
  , mEcio(aEcio)
  , mSignalNoiseRatio(aSignalNoiseRatio)
{
  DEBUG("init nsEvdoSignalStrength");
}

NS_IMETHODIMP
nsEvdoSignalStrength::GetDbm(int32_t* aDbm)
{
  *aDbm = mDbm;
  return NS_OK;
}
NS_IMETHODIMP
nsEvdoSignalStrength::GetEcio(int32_t* aEcio)
{
  *aEcio = mEcio;
  return NS_OK;
}
NS_IMETHODIMP
nsEvdoSignalStrength::GetSignalNoiseRatio(int32_t* aSignalNoiseRatio)
{
  *aSignalNoiseRatio = mSignalNoiseRatio;
  return NS_OK;
}
NS_IMPL_ISUPPORTS(nsEvdoSignalStrength, nsIEvdoSignalStrength)

/*============================================================================
 *============ Implementation of Class nsLteSignalStrength ===================
 *============================================================================*/
/**
 * nsLteSignalStrength implementation
 */
nsLteSignalStrength::nsLteSignalStrength(int32_t aSignalStrength,
                                         int32_t aRsrp,
                                         int32_t aRsrq,
                                         int32_t aRssnr,
                                         int32_t aCqi,
                                         int32_t aTimingAdvance,
                                         uint32_t aCqiTableIndex)
  : mSignalStrength(aSignalStrength)
  , mRsrp(aRsrp)
  , mRsrq(aRsrq)
  , mRssnr(aRssnr)
  , mCqi(aCqi)
  , mTimingAdvance(aTimingAdvance)
  , mCqiTableIndex(aCqiTableIndex)
{
  DEBUG("init nsLteSignalStrength");
}

NS_IMETHODIMP
nsLteSignalStrength::GetSignalStrength(int32_t* aSignalStrength)
{
  *aSignalStrength = mSignalStrength;
  return NS_OK;
}
NS_IMETHODIMP
nsLteSignalStrength::GetRsrp(int32_t* aRsrp)
{
  *aRsrp = mRsrp;
  return NS_OK;
}
NS_IMETHODIMP
nsLteSignalStrength::GetRsrq(int32_t* aRsrq)
{
  *aRsrq = mRsrq;
  return NS_OK;
}
NS_IMETHODIMP
nsLteSignalStrength::GetRssnr(int32_t* aRssnr)
{
  *aRssnr = mRssnr;
  return NS_OK;
}
NS_IMETHODIMP
nsLteSignalStrength::GetCqi(int32_t* aCqi)
{
  *aCqi = mCqi;
  return NS_OK;
}

NS_IMETHODIMP
nsLteSignalStrength::GetTimingAdvance(int32_t* aTimingAdvance)
{
  *aTimingAdvance = mTimingAdvance;
  return NS_OK;
}

NS_IMETHODIMP
nsLteSignalStrength::GetCqiTableIndex(uint32_t* aCqiTableIndex)
{
  *aCqiTableIndex = mCqiTableIndex;
  return NS_OK;
}
NS_IMPL_ISUPPORTS(nsLteSignalStrength, nsILteSignalStrength)

/*============================================================================
 *============ Implementation of Class nsCellConfigLte ===================
 *============================================================================*/
/**
 * nsLteSignalStrength implementation
 */
nsCellConfigLte::nsCellConfigLte(bool isEndcAvailable)
  : mIsEndcAvailable(isEndcAvailable)
{
  DEBUG("init nsLteSignalStrength");
}

NS_IMETHODIMP
nsCellConfigLte::GetIsEndcAvailable(bool* isEndcAvailable)
{
  *isEndcAvailable = mIsEndcAvailable;
  return NS_OK;
}
NS_IMPL_ISUPPORTS(nsCellConfigLte, nsICellConfigLte)

/*============================================================================
 *============ Implementation of Class nsTdScdmaSignalStrength
 *===================
 *============================================================================*/
/**
 * nsTdScdmaSignalStrength implementation
 */
nsTdScdmaSignalStrength::nsTdScdmaSignalStrength(int32_t aSignalStrength,
                                                 int32_t aBitErrorRate,
                                                 int32_t aRscp)
  : mSignalStrength(aSignalStrength)
  , mBitErrorRate(aBitErrorRate)
  , mRscp(aRscp)
{
  DEBUG("init nsTdScdmaSignalStrength");
}

NS_IMETHODIMP
nsTdScdmaSignalStrength::GetSignalStrength(int32_t* aSignalStrength)
{
  *aSignalStrength = mSignalStrength;
  return NS_OK;
}

NS_IMETHODIMP
nsTdScdmaSignalStrength::GetBitErrorRate(int32_t* aBitErrorRate)
{
  *aBitErrorRate = mBitErrorRate;
  return NS_OK;
}

NS_IMETHODIMP
nsTdScdmaSignalStrength::GetRscp(int32_t* aRscp)
{
  *aRscp = mRscp;
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsTdScdmaSignalStrength, nsITdScdmaSignalStrength)

/*============================================================================
 *============ Implementation of Class nsNrSignalStrength
 *===================
 *============================================================================*/
/**
 * nsNrSignalStrength implementation
 */
nsNrSignalStrength::nsNrSignalStrength(int32_t csiRsrp,
                                       int32_t csiRsrq,
                                       int32_t csiSinr,
                                       int32_t ssRsrp,
                                       int32_t ssRsrq,
                                       int32_t ssSinr,
                                       uint32_t aCsiCqiTableIndex,
                                       const nsTArray<int32_t>& aCsiCqiReport)
  : mCsiRsrp(csiRsrp)
  , mCsiRsrq(csiRsrq)
  , mCsiSinr(csiSinr)
  , mSsRsrp(ssRsrp)
  , mSsRsrq(ssRsrq)
  , mSsSinr(ssSinr)
  , mCsiCqiTableIndex(aCsiCqiTableIndex)
  , mCsiCqiReport(aCsiCqiReport.Clone())
{
  DEBUG("init nsNrSignalStrength");
}

NS_IMETHODIMP
nsNrSignalStrength::GetCsiRsrp(int32_t* aCsiRsrp)
{
  *aCsiRsrp = mCsiRsrp;
  return NS_OK;
}

NS_IMETHODIMP
nsNrSignalStrength::GetCsiRsrq(int32_t* aCsiRsrq)
{
  *aCsiRsrq = mCsiRsrq;
  return NS_OK;
}
NS_IMETHODIMP
nsNrSignalStrength::GetCsiSinr(int32_t* aCsiSinr)
{
  *aCsiSinr = mCsiSinr;
  return NS_OK;
}
NS_IMETHODIMP
nsNrSignalStrength::GetSsRsrp(int32_t* aSsRsrp)
{
  *aSsRsrp = mSsRsrp;
  return NS_OK;
}
NS_IMETHODIMP
nsNrSignalStrength::GetSsRsrq(int32_t* aSsRsrq)
{
  *aSsRsrq = mSsRsrq;
  return NS_OK;
}
NS_IMETHODIMP
nsNrSignalStrength::GetSsSinr(int32_t* aSsSinr)
{
  *aSsSinr = mSsSinr;
  return NS_OK;
}
NS_IMETHODIMP
nsNrSignalStrength::GetCsiCqiTableIndex(uint32_t* aCsiCqiTableIndex)
{
  *aCsiCqiTableIndex = mCsiCqiTableIndex;
  return NS_OK;
}
NS_IMETHODIMP
nsNrSignalStrength::GetCsiCqiReport(nsTArray<int32_t>& aCsiCqiReport)
{
  aCsiCqiReport = mCsiCqiReport.Clone();
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsNrSignalStrength, nsINrSignalStrength)

/*============================================================================
 *============ Implementation of Class nsSignalStrength ===================
 *============================================================================*/
/**
 * nsSignalStrength implementation
 */
nsSignalStrength::nsSignalStrength(
  nsGsmSignalStrength* aGsmSignalStrength,
  nsCdmaSignalStrength* aCdmaSignalStrength,
  nsEvdoSignalStrength* aEvdoSignalStrength,
  nsLteSignalStrength* aLteSignalStrength,
  nsTdScdmaSignalStrength* aTdScdmaSignalStrength,
  nsWcdmaSignalStrength* aWcdmaSignalStrength,
  nsNrSignalStrength* aNrSignalStrength)
  : mGsmSignalStrength(aGsmSignalStrength)
  , mCdmaSignalStrength(aCdmaSignalStrength)
  , mEvdoSignalStrength(aEvdoSignalStrength)
  , mLteSignalStrength(aLteSignalStrength)
  , mTdScdmaSignalStrength(aTdScdmaSignalStrength)
  , mWcdmaSignalStrength(aWcdmaSignalStrength)
  , mNrSignalStrength(aNrSignalStrength)
{
  DEBUG("init nsSignalStrength");
}

NS_IMETHODIMP
nsSignalStrength::GetGsmSignalStrength(
  nsIGsmSignalStrength** aGsmSignalStrength)
{
  RefPtr<nsIGsmSignalStrength> gsmSignalStrength(mGsmSignalStrength);
  gsmSignalStrength.forget(aGsmSignalStrength);
  return NS_OK;
}

NS_IMETHODIMP
nsSignalStrength::GetCdmaSignalStrength(
  nsICdmaSignalStrength** aCdmaSignalStrength)
{
  RefPtr<nsICdmaSignalStrength> cdmaSignalStrength(mCdmaSignalStrength);
  cdmaSignalStrength.forget(aCdmaSignalStrength);
  return NS_OK;
}

NS_IMETHODIMP
nsSignalStrength::GetEvdoSignalStrength(
  nsIEvdoSignalStrength** aEvdoSignalStrength)
{
  RefPtr<nsIEvdoSignalStrength> evdoSignalStrength(mEvdoSignalStrength);
  evdoSignalStrength.forget(aEvdoSignalStrength);
  return NS_OK;
}

NS_IMETHODIMP
nsSignalStrength::GetLteSignalStrength(
  nsILteSignalStrength** aLteSignalStrength)
{
  RefPtr<nsILteSignalStrength> lteSignalStrength(mLteSignalStrength);
  lteSignalStrength.forget(aLteSignalStrength);
  return NS_OK;
}

NS_IMETHODIMP
nsSignalStrength::GetTdscdmaSignalStrength(
  nsITdScdmaSignalStrength** aTdscdmaSignalStrength)
{
  RefPtr<nsITdScdmaSignalStrength> tdscdmaSignalStrength(
    mTdScdmaSignalStrength);
  tdscdmaSignalStrength.forget(aTdscdmaSignalStrength);
  return NS_OK;
}

NS_IMETHODIMP
nsSignalStrength::GetWcdmaSignalStrength(
  nsIWcdmaSignalStrength** aWcdmaSignalStrength)
{
  RefPtr<nsIWcdmaSignalStrength> wcdmaSignalStrength(mWcdmaSignalStrength);
  wcdmaSignalStrength.forget(aWcdmaSignalStrength);
  return NS_OK;
}

NS_IMETHODIMP
nsSignalStrength::GetNrSignalStrength(nsINrSignalStrength** aNrSignalStrength)
{
  RefPtr<nsNrSignalStrength> nrSignalStrength(mNrSignalStrength);
  nrSignalStrength.forget(aNrSignalStrength);
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsSignalStrength, nsISignalStrength)

/*============================================================================
 *============ Implementation of Class nsSetupDataCallResult ===================
 *============================================================================*/
/**
 * nsSetupDataCallResult implementation
 */
nsSetupDataCallResult::nsSetupDataCallResult(
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
  int32_t aMtuV6)
  : mFailCause(aFailCause)
  , mSuggestedRetryTime(aSuggestedRetryTime)
  , mCid(aCid)
  , mActive(aActive)
  , mPdpType(aPdpType)
  , mIfname(aIfname)
  , mAddresses(aAddresses.Clone())
  , mDnses(aDnses)
  , mGateways(aGateways)
  , mPcscf(aPcscf)
  , mMtu(aMtu)
  , mMtuV4(aMtuV4)
  , mMtuV6(aMtuV6)
{
  DEBUG("init nsSetupDataCallResult");
}

nsSetupDataCallResult::nsSetupDataCallResult(
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
  nsTArray<RefPtr<nsIQosSession>>& aQosSessions)
  : mFailCause(aFailCause)
  , mSuggestedRetryTime(aSuggestedRetryTime)
  , mCid(aCid)
  , mActive(aActive)
  , mPdpType(aPdpType)
  , mIfname(aIfname)
  , mAddresses(aAddresses.Clone())
  , mDnses(aDnses)
  , mGateways(aGateways)
  , mPcscf(aPcscf)
  , mMtu(aMtu)
  , mMtuV4(aMtuV4)
  , mMtuV6(aMtuV6)
  , pduSessionId(aPduSessionId)
  , handoverFailureMode(aHFailureMode)
  , sliceInfo(aSliceInfo)
  , defaultQos(aQos)
  , trafficDescriptors(aTrafficDescriptors.Clone())
  , qosSessions(aQosSessions.Clone())
{}

NS_IMETHODIMP
nsSetupDataCallResult::GetPduSessionId(int32_t* aParam)
{
  *aParam = pduSessionId;
  return NS_OK;
}

NS_IMETHODIMP
nsSetupDataCallResult::GetHandoverFailureMode(int32_t* aParam)
{
  *aParam = handoverFailureMode;
  return NS_OK;
}

NS_IMETHODIMP
nsSetupDataCallResult::GetSliceInfo(nsISliceInfo** aParam)
{
  RefPtr<nsISliceInfo> data(sliceInfo);
  data.forget(aParam);
  return NS_OK;
}

NS_IMETHODIMP
nsSetupDataCallResult::GetDefaultQos(nsIQos** aParam)
{
  RefPtr<nsIQos> data(defaultQos);
  data.forget(aParam);
  return NS_OK;
}

NS_IMETHODIMP
nsSetupDataCallResult::GetTrafficDescriptors(
  nsTArray<RefPtr<nsITrafficDescriptor>>& data)
{
  for (uint32_t i = 0; i < trafficDescriptors.Length(); i++) {
    data.AppendElement(trafficDescriptors[i]);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSetupDataCallResult::GetQosSessions(nsTArray<RefPtr<nsIQosSession>>& data)
{
  for (uint32_t i = 0; i < qosSessions.Length(); i++) {
    data.AppendElement(qosSessions[i]);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSetupDataCallResult::GetFailCause(int32_t* aFailCause)
{
  *aFailCause = mFailCause;
  return NS_OK;
}

NS_IMETHODIMP
nsSetupDataCallResult::GetSuggestedRetryTime(int64_t* aSuggestedRetryTime)
{
  *aSuggestedRetryTime = mSuggestedRetryTime;
  return NS_OK;
}

NS_IMETHODIMP
nsSetupDataCallResult::GetCid(int32_t* aCid)
{
  *aCid = mCid;
  return NS_OK;
}

NS_IMETHODIMP
nsSetupDataCallResult::GetActive(int32_t* aActive)
{
  *aActive = mActive;
  return NS_OK;
}

NS_IMETHODIMP
nsSetupDataCallResult::GetPdpType(nsAString& aPdpType)
{
  aPdpType = mPdpType;
  return NS_OK;
}

NS_IMETHODIMP
nsSetupDataCallResult::GetIfname(nsAString& aIfname)
{
  aIfname = mIfname;
  return NS_OK;
}

NS_IMETHODIMP
nsSetupDataCallResult::GetAddresses(
  nsTArray<RefPtr<nsILinkAddress>>& aAddresses)
{
  for (uint32_t i = 0; i < mAddresses.Length(); i++) {
    aAddresses.AppendElement(mAddresses[i]);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSetupDataCallResult::GetDnses(nsAString& aDnses)
{
  aDnses = mDnses;
  return NS_OK;
}

NS_IMETHODIMP
nsSetupDataCallResult::GetGateways(nsAString& aGateways)
{
  aGateways = mGateways;
  return NS_OK;
}

NS_IMETHODIMP
nsSetupDataCallResult::GetPcscf(nsAString& aPcscf)
{
  aPcscf = mPcscf;
  return NS_OK;
}

NS_IMETHODIMP
nsSetupDataCallResult::GetMtu(int32_t* aMtu)
{
  *aMtu = mMtu;
  return NS_OK;
}

NS_IMETHODIMP
nsSetupDataCallResult::GetMtuV4(int32_t* aMtuV4)
{
  *aMtuV4 = mMtuV4;
  return NS_OK;
}

NS_IMETHODIMP
nsSetupDataCallResult::GetMtuV6(int32_t* aMtuV6)
{
  *aMtuV6 = mMtuV6;
  return NS_OK;
}
NS_IMPL_ISUPPORTS(nsSetupDataCallResult, nsISetupDataCallResult)

/*============================================================================
 *============ Implementation of Class nsSuppSvcNotification ===================
 *============================================================================*/
/**
 * nsSuppSvcNotification implementation
 */
nsSuppSvcNotification::nsSuppSvcNotification(bool aNotificationType,
                                             int32_t aCode,
                                             int32_t aIndex,
                                             int32_t aType,
                                             const nsAString& aNumber)
  : mNotificationType(aNotificationType)
  , mCode(aCode)
  , mIndex(aIndex)
  , mType(aType)
  , mNumber(aNumber)
{
  DEBUG("init nsSuppSvcNotification");
}

NS_IMETHODIMP
nsSuppSvcNotification::GetNotificationType(bool* aNotificationType)
{
  *aNotificationType = mNotificationType;
  return NS_OK;
}

NS_IMETHODIMP
nsSuppSvcNotification::GetCode(int32_t* aCode)
{
  *aCode = mCode;
  return NS_OK;
}

NS_IMETHODIMP
nsSuppSvcNotification::GetIndex(int32_t* aIndex)
{
  *aIndex = mIndex;
  return NS_OK;
}

NS_IMETHODIMP
nsSuppSvcNotification::GetType(int32_t* aType)
{
  *aType = mType;
  return NS_OK;
}

NS_IMETHODIMP
nsSuppSvcNotification::GetNumber(nsAString& aNumber)
{
  aNumber = mNumber;
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsSuppSvcNotification, nsISuppSvcNotification)

/*============================================================================
 *============ Implementation of Class nsSimRefreshResult ===================
 *============================================================================*/
/**
 * nsSimRefreshResult implementation
 */
nsSimRefreshResult::nsSimRefreshResult(int32_t aType,
                                       int32_t aEfId,
                                       const nsAString& aAid)
  : mType(aType)
  , mEfId(aEfId)
  , mAid(aAid)
{
  DEBUG("init nsSimRefreshResult");
}

NS_IMETHODIMP
nsSimRefreshResult::GetType(int32_t* aType)
{
  *aType = mType;
  return NS_OK;
}

NS_IMETHODIMP
nsSimRefreshResult::GetEfId(int32_t* aEfId)
{
  *aEfId = mEfId;
  return NS_OK;
}

NS_IMETHODIMP
nsSimRefreshResult::GetAid(nsAString& aAid)
{
  aAid = mAid;
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsSimRefreshResult, nsISimRefreshResult)

/*============================================================================
 *============ Implementation of Class nsCellIdentityOperatorNames
 *===================
 *============================================================================*/
/**
 * nsCellIdentityOperatorNames implementation
 */
nsCellIdentityOperatorNames::nsCellIdentityOperatorNames(
  const nsAString& aAlphaLong,
  const nsAString& aAlphaShort)
  : mAlphaLong(aAlphaLong)
  , mAlphaShort(aAlphaShort)
{
  DEBUG("init nsCellIdentityOperatorNames");
}
#if ANDROID_VERSION >= 33
nsCellIdentityOperatorNames::nsCellIdentityOperatorNames(
  CellIdentityOperatorNames_V1_2& aMessage)
{
  mAlphaLong = NS_ConvertUTF8toUTF16(aMessage.alphaLong.c_str());
  mAlphaShort = NS_ConvertUTF8toUTF16(aMessage.alphaShort.c_str());
}
#endif
NS_IMETHODIMP
nsCellIdentityOperatorNames::GetAlphaLong(nsAString& aAlphaLong)
{
  aAlphaLong = mAlphaLong;
  return NS_OK;
}
NS_IMETHODIMP
nsCellIdentityOperatorNames::GetAlphaShort(nsAString& aAlphaShort)
{
  aAlphaShort = mAlphaShort;
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsCellIdentityOperatorNames, nsICellIdentityOperatorNames)

/*============================================================================
 *============ Implementation of Class nsCellIdentityOperatorNames
 *===================
 *============================================================================*/
/**
 * nsCellIdentityOperatorNames implementation
 */
nsCellIdentityCsgInfo::nsCellIdentityCsgInfo(bool aCsgIndication,
                                             const nsAString& aHomeNodebName,
                                             int32_t aCsgIdentity)
  : mCsgIndication(aCsgIndication)
  , mHomeNodebName(aHomeNodebName)
  , mCsgIdentity(aCsgIdentity)
{
  DEBUG("init nsCellIdentityCsgInfo");
}

NS_IMETHODIMP
nsCellIdentityCsgInfo::GetCsgIndication(bool* aCsgIndication)
{
  *aCsgIndication = mCsgIndication;
  return NS_OK;
}
NS_IMETHODIMP
nsCellIdentityCsgInfo::GetHomeNodebName(nsAString& aHomeNodebName)
{
  aHomeNodebName = mHomeNodebName;
  return NS_OK;
}
NS_IMETHODIMP
nsCellIdentityCsgInfo::GetCsgIdentity(int32_t* aCsgIdentity)
{
  *aCsgIdentity = mCsgIdentity;
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsCellIdentityCsgInfo, nsICellIdentityCsgInfo)

/*============================================================================
 *============ Implementation of Class nsCellIdentityGsm ===================
 *============================================================================*/
/**
 * nsCellIdentityGsm implementation
 */
nsCellIdentityGsm::nsCellIdentityGsm(
  const nsAString& aMcc,
  const nsAString& aMnc,
  int32_t aLac,
  int32_t aCid,
  int32_t aArfcn,
  int32_t aBsic,
  nsCellIdentityOperatorNames* aOperatorNames,
  nsTArray<nsString>& aAdditionalPlmns)
  : mMcc(aMcc)
  , mMnc(aMnc)
  , mLac(aLac)
  , mCid(aCid)
  , mArfcn(aArfcn)
  , mBsic(aBsic)
  , mOperatorNames(aOperatorNames)
{
  DEBUG("init nsCellIdentityGsm");
  for (uint32_t i = 0; i < aAdditionalPlmns.Length(); i++) {
    mAdditionalPlmns.AppendElement(aAdditionalPlmns[i]);
  }
}

NS_IMETHODIMP
nsCellIdentityGsm::GetMcc(nsAString& aMcc)
{
  aMcc = mMcc;
  return NS_OK;
}
NS_IMETHODIMP
nsCellIdentityGsm::GetMnc(nsAString& aMnc)
{
  aMnc = mMnc;
  return NS_OK;
}
NS_IMETHODIMP
nsCellIdentityGsm::GetLac(int32_t* aLac)
{
  *aLac = mLac;
  return NS_OK;
}
NS_IMETHODIMP
nsCellIdentityGsm::GetCid(int32_t* aCid)
{
  *aCid = mCid;
  return NS_OK;
}

NS_IMETHODIMP
nsCellIdentityGsm::GetArfcn(int32_t* aArfcn)
{
  *aArfcn = mArfcn;
  return NS_OK;
}

NS_IMETHODIMP
nsCellIdentityGsm::GetBsic(int32_t* aBsic)
{
  *aBsic = mBsic;
  return NS_OK;
}

NS_IMETHODIMP
nsCellIdentityGsm::GetOperatorNames(
  nsICellIdentityOperatorNames** aOperatorNames)
{
  RefPtr<nsICellIdentityOperatorNames> operatorNames(mOperatorNames);
  operatorNames.forget(aOperatorNames);
  return NS_OK;
}

NS_IMETHODIMP
nsCellIdentityGsm::GetAdditionalPlmns(nsTArray<nsString>& additionalPlmns)
{
  additionalPlmns = mAdditionalPlmns.Clone();
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsCellIdentityGsm, nsICellIdentityGsm)

/*============================================================================
 *============ Implementation of Class nsCellIdentityCdma ===================
 *============================================================================*/
/**
 * nsCellIdentityCdma implementation
 */
nsCellIdentityCdma::nsCellIdentityCdma(
  int32_t aNetworkId,
  int32_t aSystemId,
  int32_t aBaseStationId,
  int32_t aLongitude,
  int32_t aLatitude,
  nsCellIdentityOperatorNames* aOperatorNames)
  : mNetworkId(aNetworkId)
  , mSystemId(aSystemId)
  , mBaseStationId(aBaseStationId)
  , mLongitude(aLongitude)
  , mLatitude(aLatitude)
{
  DEBUG("init nsCellIdentityCdma");
}

NS_IMETHODIMP
nsCellIdentityCdma::GetNetworkId(int32_t* aNetworkId)
{
  *aNetworkId = mNetworkId;
  return NS_OK;
}
NS_IMETHODIMP
nsCellIdentityCdma::GetSystemId(int32_t* aSystemId)
{
  *aSystemId = mSystemId;
  return NS_OK;
}

NS_IMETHODIMP
nsCellIdentityCdma::GetBaseStationId(int32_t* aBaseStationId)
{
  *aBaseStationId = mBaseStationId;
  return NS_OK;
}

NS_IMETHODIMP
nsCellIdentityCdma::GetLongitude(int32_t* aLongitude)
{
  *aLongitude = mLongitude;
  return NS_OK;
}

NS_IMETHODIMP
nsCellIdentityCdma::GetLatitude(int32_t* aLatitude)
{
  *aLatitude = mLatitude;
  return NS_OK;
}

NS_IMETHODIMP
nsCellIdentityCdma::GetOperatorNames(
  nsICellIdentityOperatorNames** aOperatorNames)
{
  RefPtr<nsICellIdentityOperatorNames> operatorNames(mOperatorNames);
  operatorNames.forget(aOperatorNames);
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsCellIdentityCdma, nsICellIdentityCdma)

/*============================================================================
 *============ Implementation of Class nsCellIdentityLte ===================
 *============================================================================*/
/**
 * nsCellIdentityLte implementation
 */
nsCellIdentityLte::nsCellIdentityLte(
  const nsAString& aMcc,
  const nsAString& aMnc,
  int32_t aCi,
  int32_t aPci,
  int32_t aTac,
  int32_t aEarfcn,
  nsCellIdentityOperatorNames* aOperatorNames,
  int32_t aBandwidth,
  nsTArray<nsString>& aAdditionalPlmns,
  nsCellIdentityCsgInfo* aCsgInfo,
  nsTArray<int32_t>& aBands)
  : mMcc(aMcc)
  , mMnc(aMnc)
  , mCi(aCi)
  , mPci(aPci)
  , mTac(aTac)
  , mEarfcn(aEarfcn)
  , mOperatorNames(aOperatorNames)
  , mBandwidth(aBandwidth)
  , mCsgInfo(aCsgInfo)
  , mBands(aBands.Clone())
{
  DEBUG("init nsCellIdentityLte");
  for (uint32_t i = 0; i < aAdditionalPlmns.Length(); i++) {
    mAdditionalPlmns.AppendElement(aAdditionalPlmns[i]);
  }
}

NS_IMETHODIMP
nsCellIdentityLte::GetMcc(nsAString& aMcc)
{
  aMcc = mMcc;
  return NS_OK;
}
NS_IMETHODIMP
nsCellIdentityLte::GetMnc(nsAString& aMnc)
{
  aMnc = mMnc;
  return NS_OK;
}
NS_IMETHODIMP
nsCellIdentityLte::GetCi(int32_t* aCi)
{
  *aCi = mCi;
  return NS_OK;
}
NS_IMETHODIMP
nsCellIdentityLte::GetPci(int32_t* aPci)
{
  *aPci = mPci;
  return NS_OK;
}

NS_IMETHODIMP
nsCellIdentityLte::GetTac(int32_t* aTac)
{
  *aTac = mTac;
  return NS_OK;
}

NS_IMETHODIMP
nsCellIdentityLte::GetEarfcn(int32_t* aEarfcn)
{
  *aEarfcn = mEarfcn;
  return NS_OK;
}

NS_IMETHODIMP
nsCellIdentityLte::GetOperatorNames(
  nsICellIdentityOperatorNames** aOperatorNames)
{
  RefPtr<nsICellIdentityOperatorNames> operatorNames(mOperatorNames);
  operatorNames.forget(aOperatorNames);
  return NS_OK;
}

NS_IMETHODIMP
nsCellIdentityLte::GetBandwidth(int32_t* aBandwidth)
{
  *aBandwidth = mBandwidth;
  return NS_OK;
}

NS_IMETHODIMP
nsCellIdentityLte::GetAdditionalPlmns(nsTArray<nsString>& additionalPlmns)
{
  additionalPlmns = mAdditionalPlmns.Clone();
  return NS_OK;
}

NS_IMETHODIMP
nsCellIdentityLte::GetCsgInfo(nsICellIdentityCsgInfo** aCsgInfo)
{
  RefPtr<nsICellIdentityCsgInfo> csgInfo(mCsgInfo);
  csgInfo.forget(aCsgInfo);
  return NS_OK;
}

NS_IMETHODIMP
nsCellIdentityLte::GetBands(nsTArray<int32_t>& aBands)
{
  aBands = mBands.Clone();
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsCellIdentityLte, nsICellIdentityLte)

/*============================================================================
 *============ Implementation of Class nsCellIdentityWcdma ===================
 *============================================================================*/
/**
 * nsCellIdentityWcdma implementation
 */
nsCellIdentityWcdma::nsCellIdentityWcdma(
  const nsAString& aMcc,
  const nsAString& aMnc,
  int32_t aLac,
  int32_t aCid,
  int32_t aPsc,
  int32_t aUarfcn,
  nsCellIdentityOperatorNames* aOperatorNames,
  nsTArray<nsString>& aAdditionalPlmns,
  nsCellIdentityCsgInfo* aCsgInfo)
  : mMcc(aMcc)
  , mMnc(aMnc)
  , mLac(aLac)
  , mCid(aCid)
  , mPsc(aPsc)
  , mUarfcn(aUarfcn)
  , mOperatorNames(aOperatorNames)
  , mCsgInfo(aCsgInfo)
{
  DEBUG("init nsCellIdentityWcdma");
  for (uint32_t i = 0; i < aAdditionalPlmns.Length(); i++) {
    mAdditionalPlmns.AppendElement(aAdditionalPlmns[i]);
  }
}

NS_IMETHODIMP
nsCellIdentityWcdma::GetMcc(nsAString& aMcc)
{
  aMcc = mMcc;
  return NS_OK;
}
NS_IMETHODIMP
nsCellIdentityWcdma::GetMnc(nsAString& aMnc)
{
  aMnc = mMnc;
  return NS_OK;
}
NS_IMETHODIMP
nsCellIdentityWcdma::GetLac(int32_t* aLac)
{
  *aLac = mLac;
  return NS_OK;
}
NS_IMETHODIMP
nsCellIdentityWcdma::GetCid(int32_t* aCid)
{
  *aCid = mCid;
  return NS_OK;
}

NS_IMETHODIMP
nsCellIdentityWcdma::GetPsc(int32_t* aPsc)
{
  *aPsc = mPsc;
  return NS_OK;
}

NS_IMETHODIMP
nsCellIdentityWcdma::GetUarfcn(int32_t* aUarfcn)
{
  *aUarfcn = mUarfcn;
  return NS_OK;
}

NS_IMETHODIMP
nsCellIdentityWcdma::GetOperatorNames(
  nsICellIdentityOperatorNames** aOperatorNames)
{
  RefPtr<nsICellIdentityOperatorNames> operatorNames(mOperatorNames);
  operatorNames.forget(aOperatorNames);
  return NS_OK;
}

NS_IMETHODIMP
nsCellIdentityWcdma::GetAdditionalPlmns(nsTArray<nsString>& additionalPlmns)
{
  additionalPlmns = mAdditionalPlmns.Clone();
  return NS_OK;
}

NS_IMETHODIMP
nsCellIdentityWcdma::GetCsgInfo(nsICellIdentityCsgInfo** aCsgInfo)
{
  RefPtr<nsICellIdentityCsgInfo> csgInfo(mCsgInfo);
  csgInfo.forget(aCsgInfo);
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsCellIdentityWcdma, nsICellIdentityWcdma)

/*============================================================================
 *============ Implementation of Class nsCellIdentityTdScdma ===================
 *============================================================================*/
/**
 * nsCellIdentityTdScdma implementation
 */
nsCellIdentityTdScdma::nsCellIdentityTdScdma(
  const nsAString& aMcc,
  const nsAString& aMnc,
  int32_t aLac,
  int32_t aCid,
  int32_t aCpid,
  nsCellIdentityOperatorNames* aOperatorNames,
  int32_t aUarfcn,
  nsTArray<nsString>& aAdditionalPlmns,
  nsCellIdentityCsgInfo* aCsgInfo)
  : mMcc(aMcc)
  , mMnc(aMnc)
  , mLac(aLac)
  , mCid(aCid)
  , mCpid(aCpid)
  , mCsgInfo(aCsgInfo)
{
  DEBUG("init nsCellIdentityTdScdma");
  for (uint32_t i = 0; i < aAdditionalPlmns.Length(); i++) {
    mAdditionalPlmns.AppendElement(aAdditionalPlmns[i]);
  }
}

NS_IMETHODIMP
nsCellIdentityTdScdma::GetMcc(nsAString& aMcc)
{
  aMcc = mMcc;
  return NS_OK;
}

NS_IMETHODIMP
nsCellIdentityTdScdma::GetMnc(nsAString& aMnc)
{
  aMnc = mMnc;
  return NS_OK;
}

NS_IMETHODIMP
nsCellIdentityTdScdma::GetLac(int32_t* aLac)
{
  *aLac = mLac;
  return NS_OK;
}

NS_IMETHODIMP
nsCellIdentityTdScdma::GetCid(int32_t* aCid)
{
  *aCid = mCid;
  return NS_OK;
}

NS_IMETHODIMP
nsCellIdentityTdScdma::GetCpid(int32_t* aCpid)
{
  *aCpid = mCpid;
  return NS_OK;
}

NS_IMETHODIMP
nsCellIdentityTdScdma::GetOperatorNames(
  nsICellIdentityOperatorNames** aOperatorNames)
{
  RefPtr<nsICellIdentityOperatorNames> operatorNames(mOperatorNames);
  operatorNames.forget(aOperatorNames);
  return NS_OK;
}

NS_IMETHODIMP
nsCellIdentityTdScdma::GetUarfcn(int32_t* aUarfcn)
{
  *aUarfcn = mUarfcn;
  return NS_OK;
}

NS_IMETHODIMP
nsCellIdentityTdScdma::GetAdditionalPlmns(nsTArray<nsString>& additionalPlmns)
{
  additionalPlmns = mAdditionalPlmns.Clone();
  return NS_OK;
}

NS_IMETHODIMP
nsCellIdentityTdScdma::GetCsgInfo(nsICellIdentityCsgInfo** aCsgInfo)
{
  RefPtr<nsICellIdentityCsgInfo> csgInfo(mCsgInfo);
  csgInfo.forget(aCsgInfo);
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsCellIdentityTdScdma, nsICellIdentityTdScdma)

nsCellIdentityNr::nsCellIdentityNr(const nsAString& aMcc,
                                   const nsAString& aMnc,
                                   uint64_t aNci,
                                   uint32_t aPci,
                                   int32_t aTac,
                                   int32_t aNrarfcn,
                                   nsCellIdentityOperatorNames* aOperatorNames,
                                   nsTArray<nsString>& aAdditionalPlmns,
                                   nsTArray<int32_t>& aBands)
  : mMcc(aMcc)
  , mMnc(aMnc)
  , mNci(aNci)
  , mPci(aPci)
  , mTac(aTac)
  , mNrarfcn(aNrarfcn)
  , mOperatorNames(aOperatorNames)
  , mBands(aBands.Clone())
{
  DEBUG("init nsCellIdentityNr");
  for (uint32_t i = 0; i < aAdditionalPlmns.Length(); i++) {
    mAdditionalPlmns.AppendElement(aAdditionalPlmns[i]);
  }
}

NS_IMETHODIMP
nsCellIdentityNr::GetMcc(nsAString& aMcc)
{
  aMcc = mMcc;
  return NS_OK;
}

NS_IMETHODIMP
nsCellIdentityNr::GetMnc(nsAString& aMnc)
{
  aMnc = mMnc;
  return NS_OK;
}

NS_IMETHODIMP
nsCellIdentityNr::GetNci(uint64_t* aNci)
{
  *aNci = mNci;
  return NS_OK;
}

NS_IMETHODIMP
nsCellIdentityNr::GetPci(uint32_t* aPci)
{
  *aPci = mPci;
  return NS_OK;
}

NS_IMETHODIMP
nsCellIdentityNr::GetTac(int32_t* aTac)
{
  *aTac = mTac;
  return NS_OK;
}

NS_IMETHODIMP
nsCellIdentityNr::GetNrarfcn(int32_t* aNrarfcn)
{
  *aNrarfcn = mNrarfcn;
  return NS_OK;
}

NS_IMETHODIMP
nsCellIdentityNr::GetOperatorNames(
  nsICellIdentityOperatorNames** aOperatorNames)
{
  RefPtr<nsICellIdentityOperatorNames> operatorNames(mOperatorNames);
  operatorNames.forget(aOperatorNames);
  return NS_OK;
}

NS_IMETHODIMP
nsCellIdentityNr::GetAdditionalPlmns(nsTArray<nsString>& additionalPlmns)
{
  additionalPlmns = mAdditionalPlmns.Clone();
  return NS_OK;
}

NS_IMETHODIMP
nsCellIdentityNr::GetBands(nsTArray<int32_t>& aBands)
{
  aBands = mBands.Clone();
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsCellIdentityNr, nsICellIdentityNr)
/*============================================================================
 *============ Implementation of Class nsCellIdentity ===================
 *============================================================================*/
/**
 * nsCellIdentity implementation
 */
nsCellIdentity::nsCellIdentity(int32_t aCellInfoType,
                               nsCellIdentityGsm* aCellIdentityGsm,
                               nsCellIdentityWcdma* aCellIdentityWcdma,
                               nsCellIdentityCdma* aCellIdentityCdma,
                               nsCellIdentityLte* aCellIdentityLte,
                               nsCellIdentityTdScdma* aCellIdentityTdScdma)
  : mCellInfoType(aCellInfoType)
  , mCellIdentityGsm(aCellIdentityGsm)
  , mCellIdentityWcdma(aCellIdentityWcdma)
  , mCellIdentityCdma(aCellIdentityCdma)
  , mCellIdentityLte(aCellIdentityLte)
  , mCellIdentityTdScdma(aCellIdentityTdScdma)
{
  DEBUG("init nsCellIdentity");
}

nsCellIdentity::nsCellIdentity(int32_t aCellInfoType,
                               nsCellIdentityGsm* aCellIdentityGsm)
  : mCellInfoType(aCellInfoType)
  , mCellIdentityGsm(aCellIdentityGsm)
{
  DEBUG("init nsCellIdentity GSM");
}

nsCellIdentity::nsCellIdentity(int32_t aCellInfoType,
                               nsCellIdentityWcdma* aCellIdentityWcdma)
  : mCellInfoType(aCellInfoType)
  , mCellIdentityWcdma(aCellIdentityWcdma)
{
  DEBUG("init nsCellIdentity WCDMA");
}

nsCellIdentity::nsCellIdentity(int32_t aCellInfoType,
                               nsCellIdentityCdma* aCellIdentityCdma)
  : mCellInfoType(aCellInfoType)
  , mCellIdentityCdma(aCellIdentityCdma)
{
  DEBUG("init nsCellIdentity CDMA");
}

nsCellIdentity::nsCellIdentity(int32_t aCellInfoType,
                               nsCellIdentityLte* aCellIdentityLte)
  : mCellInfoType(aCellInfoType)
  , mCellIdentityLte(aCellIdentityLte)
{
  DEBUG("init nsCellIdentity LTE");
}

nsCellIdentity::nsCellIdentity(int32_t aCellInfoType,
                               nsCellIdentityTdScdma* aCellIdentityTdScdma)
  : mCellInfoType(aCellInfoType)
  , mCellIdentityTdScdma(aCellIdentityTdScdma)
{
  DEBUG("init nsCellIdentity TDCDMA");
}

nsCellIdentity::nsCellIdentity(int32_t aCellInfoType,
                               nsCellIdentityNr* aCellIdentityNr)
  : mCellInfoType(aCellInfoType)
  , mCellIdentityNr(aCellIdentityNr)
{
  DEBUG("init nsCellIdentity NR");
}

NS_IMETHODIMP
nsCellIdentity::GetCellInfoType(int32_t* aCellInfoType)
{
  *aCellInfoType = mCellInfoType;
  return NS_OK;
}

NS_IMETHODIMP
nsCellIdentity::GetCellIdentityGsm(nsICellIdentityGsm** aCellIdentityGsm)
{
  RefPtr<nsICellIdentityGsm> cellIdentityGsm(mCellIdentityGsm);
  cellIdentityGsm.forget(aCellIdentityGsm);
  return NS_OK;
}

NS_IMETHODIMP
nsCellIdentity::GetCellIdentityWcdma(nsICellIdentityWcdma** aCellIdentityWcdma)
{
  RefPtr<nsICellIdentityWcdma> cellIdentityWcdma(mCellIdentityWcdma);
  cellIdentityWcdma.forget(aCellIdentityWcdma);
  return NS_OK;
}

NS_IMETHODIMP
nsCellIdentity::GetCellIdentityCdma(nsICellIdentityCdma** aCellIdentityCdma)
{
  RefPtr<nsICellIdentityCdma> cellIdentityCdma(mCellIdentityCdma);
  cellIdentityCdma.forget(aCellIdentityCdma);
  return NS_OK;
}

NS_IMETHODIMP
nsCellIdentity::GetCellIdentityLte(nsICellIdentityLte** aCellIdentityLte)
{
  RefPtr<nsICellIdentityLte> cellIdentityLte(mCellIdentityLte);
  cellIdentityLte.forget(aCellIdentityLte);
  return NS_OK;
}

NS_IMETHODIMP
nsCellIdentity::GetCellIdentityTdScdma(
  nsICellIdentityTdScdma** aCellIdentityTdScdma)
{
  RefPtr<nsICellIdentityTdScdma> cellIdentityTdScdma(mCellIdentityTdScdma);
  cellIdentityTdScdma.forget(aCellIdentityTdScdma);
  return NS_OK;
}

NS_IMETHODIMP
nsCellIdentity::GetCellIdentityNr(nsICellIdentityNr** aCellIdentityNr)
{
  RefPtr<nsICellIdentityNr> cellIdentityNr(mCellIdentityNr);
  cellIdentityNr.forget(aCellIdentityNr);
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsCellIdentity, nsICellIdentity)

/*============================================================================
 *============ Implementation of Class nsCellInfoGsm ===================
 *============================================================================*/
/**
 * nsCellInfoGsm implementation
 */
nsCellInfoGsm::nsCellInfoGsm(nsCellIdentityGsm* aCellIdentityGsm,
                             nsGsmSignalStrength* aSignalStrengthGsm)
  : mCellIdentityGsm(aCellIdentityGsm)
  , mSignalStrengthGsm(aSignalStrengthGsm)
{
  DEBUG("init nsCellInfoGsm");
}

NS_IMETHODIMP
nsCellInfoGsm::GetCellIdentityGsm(nsICellIdentityGsm** aCellIdentityGsm)
{
  RefPtr<nsICellIdentityGsm> cellIdentityGsm(mCellIdentityGsm);
  cellIdentityGsm.forget(aCellIdentityGsm);
  return NS_OK;
}

NS_IMETHODIMP
nsCellInfoGsm::GetSignalStrengthGsm(nsIGsmSignalStrength** aSignalStrengthGsm)
{
  RefPtr<nsIGsmSignalStrength> signalStrengthGsm(mSignalStrengthGsm);
  signalStrengthGsm.forget(aSignalStrengthGsm);
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsCellInfoGsm, nsICellInfoGsm)

/*============================================================================
 *============ Implementation of Class nsCellInfoCdma ===================
 *============================================================================*/
/**
 * nsCellInfoCdma implementation
 */
nsCellInfoCdma::nsCellInfoCdma(nsCellIdentityCdma* aCellIdentityCdma,
                               nsCdmaSignalStrength* aSignalStrengthCdma,
                               nsEvdoSignalStrength* aSignalStrengthEvdo)
  : mCellIdentityCdma(aCellIdentityCdma)
  , mSignalStrengthCdma(aSignalStrengthCdma)
  , mSignalStrengthEvdo(aSignalStrengthEvdo)
{
  DEBUG("init nsCellInfoCdma");
}

NS_IMETHODIMP
nsCellInfoCdma::GetCellIdentityCdma(nsICellIdentityCdma** aCellIdentityCdma)
{
  RefPtr<nsICellIdentityCdma> cellIdentityCdma(mCellIdentityCdma);
  cellIdentityCdma.forget(aCellIdentityCdma);
  return NS_OK;
}

NS_IMETHODIMP
nsCellInfoCdma::GetSignalStrengthCdma(
  nsICdmaSignalStrength** aSignalStrengthCdma)
{
  RefPtr<nsICdmaSignalStrength> signalStrengthCdma(mSignalStrengthCdma);
  signalStrengthCdma.forget(aSignalStrengthCdma);
  return NS_OK;
}

NS_IMETHODIMP
nsCellInfoCdma::GetSignalStrengthEvdo(
  nsIEvdoSignalStrength** aSignalStrengthEvdo)
{
  RefPtr<nsIEvdoSignalStrength> signalStrengthEvdo(mSignalStrengthEvdo);
  signalStrengthEvdo.forget(aSignalStrengthEvdo);
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsCellInfoCdma, nsICellInfoCdma)

/*============================================================================
 *============ Implementation of Class nsCellInfoLte ===================
 *============================================================================*/
/**
 * nsCellInfoLte implementation
 */
nsCellInfoLte::nsCellInfoLte(nsCellIdentityLte* aCellIdentityLte,
                             nsLteSignalStrength* aSignalStrengthLte,
                             nsCellConfigLte* aCellConfigLte)
  : mCellIdentityLte(aCellIdentityLte)
  , mSignalStrengthLte(aSignalStrengthLte)
  , mCellConfigLte(aCellConfigLte)
{
  DEBUG("init nsCellInfoLte");
}

NS_IMETHODIMP
nsCellInfoLte::GetCellIdentityLte(nsICellIdentityLte** aCellIdentityLte)
{
  RefPtr<nsICellIdentityLte> cellIdentityLte(mCellIdentityLte);
  cellIdentityLte.forget(aCellIdentityLte);
  return NS_OK;
}

NS_IMETHODIMP
nsCellInfoLte::GetSignalStrengthLte(nsILteSignalStrength** aSignalStrengthLte)
{
  RefPtr<nsILteSignalStrength> signalStrengthLte(mSignalStrengthLte);
  signalStrengthLte.forget(aSignalStrengthLte);
  return NS_OK;
}

NS_IMETHODIMP
nsCellInfoLte::GetCellConfig(nsICellConfigLte** aCellConfigLte)
{
  RefPtr<nsICellConfigLte> cellConfigLte(mCellConfigLte);
  cellConfigLte.forget(aCellConfigLte);
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsCellInfoLte, nsICellInfoLte)

/*============================================================================
 *============ Implementation of Class nsCellInfoWcdma ===================
 *============================================================================*/
/**
 * nsCellInfoWcdma implementation
 */
nsCellInfoWcdma::nsCellInfoWcdma(nsCellIdentityWcdma* aCellIdentityWcdma,
                                 nsWcdmaSignalStrength* aSignalStrengthWcdma)
  : mCellIdentityWcdma(aCellIdentityWcdma)
  , mSignalStrengthWcdma(aSignalStrengthWcdma)
{
  DEBUG("init nsCellInfoWcdma");
}

NS_IMETHODIMP
nsCellInfoWcdma::GetCellIdentityWcdma(nsICellIdentityWcdma** aCellIdentityWcdma)
{
  RefPtr<nsICellIdentityWcdma> cellIdentityWcdma(mCellIdentityWcdma);
  cellIdentityWcdma.forget(aCellIdentityWcdma);
  return NS_OK;
}

NS_IMETHODIMP
nsCellInfoWcdma::GetSignalStrengthWcdma(
  nsIWcdmaSignalStrength** aSignalStrengthWcdma)
{
  RefPtr<nsIWcdmaSignalStrength> signalStrengthWcdma(mSignalStrengthWcdma);
  signalStrengthWcdma.forget(aSignalStrengthWcdma);
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsCellInfoWcdma, nsICellInfoWcdma)

/*============================================================================
 *============ Implementation of Class nsCellInfoTdScdma ===================
 *============================================================================*/
/**
 * nsCellInfoTdScdma implementation
 */
nsCellInfoTdScdma::nsCellInfoTdScdma(
  nsCellIdentityTdScdma* aCellIdentityTdScdma,
  nsTdScdmaSignalStrength* aSignalStrengthTdScdma)
  : mCellIdentityTdScdma(aCellIdentityTdScdma)
  , mSignalStrengthTdScdma(aSignalStrengthTdScdma)
{
  DEBUG("init nsCellInfoTdScdma");
}

NS_IMETHODIMP
nsCellInfoTdScdma::GetCellIdentityTdScdma(
  nsICellIdentityTdScdma** aCellIdentityTdScdma)
{
  RefPtr<nsICellIdentityTdScdma> cellIdentityTdScdma(mCellIdentityTdScdma);
  cellIdentityTdScdma.forget(aCellIdentityTdScdma);
  return NS_OK;
}

NS_IMETHODIMP
nsCellInfoTdScdma::GetSignalStrengthTdScdma(
  nsITdScdmaSignalStrength** aSignalStrengthTdScdma)
{
  RefPtr<nsITdScdmaSignalStrength> signalStrengthTdScdma(
    mSignalStrengthTdScdma);
  signalStrengthTdScdma.forget(aSignalStrengthTdScdma);
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsCellInfoTdScdma, nsICellInfoTdScdma)

nsCellInfoNr::nsCellInfoNr(nsCellIdentityNr* aCellIdentityNr,
                           nsNrSignalStrength* aSignalStrengthNr)
  : mCellIdentityNr(aCellIdentityNr)
  , mSignalStrengthNr(aSignalStrengthNr)
{
  DEBUG("init nsCellInfoNr");
}

NS_IMETHODIMP
nsCellInfoNr::GetCellIdentityNr(nsICellIdentityNr** aCellIdentityNr)
{
  RefPtr<nsICellIdentityNr> cellIdentityNr(mCellIdentityNr);
  cellIdentityNr.forget(aCellIdentityNr);
  return NS_OK;
}

NS_IMETHODIMP
nsCellInfoNr::GetSignalStrengthNr(nsINrSignalStrength** aSignalStrengthNr)
{
  RefPtr<nsINrSignalStrength> signalStrengthNr(mSignalStrengthNr);
  signalStrengthNr.forget(aSignalStrengthNr);
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsCellInfoNr, nsICellInfoNr)
/*============================================================================
 *============ Implementation of Class nsRilCellInfo ===================
 *============================================================================*/
/**
 * nsRilCellInfo implementation
 */
nsRilCellInfo::nsRilCellInfo(int32_t aCellInfoType,
                             bool aRegistered,
                             int32_t aTimeStampType,
                             uint64_t aTimeStamp,
                             nsCellInfoGsm* aGsm,
                             nsCellInfoCdma* aCdma,
                             nsCellInfoLte* aLte,
                             nsCellInfoWcdma* aWcdma,
                             nsCellInfoTdScdma* aTdScdma,
                             int32_t aConnectionStatus)
  : mCellInfoType(aCellInfoType)
  , mRegistered(aRegistered)
  , mTimeStampType(aTimeStampType)
  , mTimeStamp(aTimeStamp)
  , mGsm(aGsm)
  , mCdma(aCdma)
  , mLte(aLte)
  , mWcdma(aWcdma)
  , mTdScdma(aTdScdma)
  , mConnectionStatus(aConnectionStatus)
{
  DEBUG("init nsRilCellInfo");
}

nsRilCellInfo::nsRilCellInfo(int32_t aCellInfoType,
                             bool aRegistered,
                             int32_t aTimeStampType,
                             uint64_t aTimeStamp,
                             nsCellInfoGsm* aGsm,
                             int32_t aConnectionStatus)
  : mCellInfoType(aCellInfoType)
  , mRegistered(aRegistered)
  , mTimeStampType(aTimeStampType)
  , mTimeStamp(aTimeStamp)
  , mGsm(aGsm)
  , mConnectionStatus(aConnectionStatus)
{
  DEBUG("init nsRilCellInfo GSM");
}

nsRilCellInfo::nsRilCellInfo(int32_t aCellInfoType,
                             bool aRegistered,
                             int32_t aTimeStampType,
                             uint64_t aTimeStamp,
                             nsCellInfoWcdma* aWcdma,
                             int32_t aConnectionStatus)
  : mCellInfoType(aCellInfoType)
  , mRegistered(aRegistered)
  , mTimeStampType(aTimeStampType)
  , mTimeStamp(aTimeStamp)
  , mWcdma(aWcdma)
  , mConnectionStatus(aConnectionStatus)
{
  DEBUG("init nsRilCellInfo WCDMA");
}

nsRilCellInfo::nsRilCellInfo(int32_t aCellInfoType,
                             bool aRegistered,
                             int32_t aTimeStampType,
                             uint64_t aTimeStamp,
                             nsCellInfoCdma* aCdma,
                             int32_t aConnectionStatus)
  : mCellInfoType(aCellInfoType)
  , mRegistered(aRegistered)
  , mTimeStampType(aTimeStampType)
  , mTimeStamp(aTimeStamp)
  , mCdma(aCdma)
  , mConnectionStatus(aConnectionStatus)
{
  DEBUG("init nsRilCellInfo CDMA");
}

nsRilCellInfo::nsRilCellInfo(int32_t aCellInfoType,
                             bool aRegistered,
                             int32_t aTimeStampType,
                             uint64_t aTimeStamp,
                             nsCellInfoLte* aLte,
                             int32_t aConnectionStatus)
  : mCellInfoType(aCellInfoType)
  , mRegistered(aRegistered)
  , mTimeStampType(aTimeStampType)
  , mTimeStamp(aTimeStamp)
  , mLte(aLte)
  , mConnectionStatus(aConnectionStatus)
{
  DEBUG("init nsRilCellInfo LTE");
}

nsRilCellInfo::nsRilCellInfo(int32_t aCellInfoType,
                             bool aRegistered,
                             int32_t aTimeStampType,
                             uint64_t aTimeStamp,
                             nsCellInfoTdScdma* aTdScdma,
                             int32_t aConnectionStatus)
  : mCellInfoType(aCellInfoType)
  , mRegistered(aRegistered)
  , mTimeStampType(aTimeStampType)
  , mTimeStamp(aTimeStamp)
  , mTdScdma(aTdScdma)
  , mConnectionStatus(aConnectionStatus)
{
  DEBUG("init nsRilCellInfo TDCDMA");
}

nsRilCellInfo::nsRilCellInfo(int32_t aCellInfoType,
                             bool aRegistered,
                             int32_t aTimeStampType,
                             uint64_t aTimeStamp,
                             nsCellInfoNr* aNr,
                             int32_t aConnectionStatus)
  : mCellInfoType(aCellInfoType)
  , mRegistered(aRegistered)
  , mTimeStampType(aTimeStampType)
  , mTimeStamp(aTimeStamp)
  , mNr(aNr)
  , mConnectionStatus(aConnectionStatus)
{
  DEBUG("init nsRilCellInfo TDCDMA");
}

NS_IMETHODIMP
nsRilCellInfo::GetCellInfoType(int32_t* aCellInfoType)
{
  *aCellInfoType = mCellInfoType;
  return NS_OK;
}

NS_IMETHODIMP
nsRilCellInfo::GetRegistered(bool* aRegistered)
{
  *aRegistered = mRegistered;
  return NS_OK;
}

NS_IMETHODIMP
nsRilCellInfo::GetTimeStampType(int32_t* aTimeStampType)
{
  *aTimeStampType = mTimeStampType;
  return NS_OK;
}

NS_IMETHODIMP
nsRilCellInfo::GetTimeStamp(uint64_t* aTimeStamp)
{
  *aTimeStamp = mTimeStamp;
  return NS_OK;
}

NS_IMETHODIMP
nsRilCellInfo::GetGsm(nsICellInfoGsm** aGsm)
{
  RefPtr<nsICellInfoGsm> cellInfoGsm(mGsm);
  cellInfoGsm.forget(aGsm);
  return NS_OK;
}

NS_IMETHODIMP
nsRilCellInfo::GetCdma(nsICellInfoCdma** aCdma)
{
  RefPtr<nsICellInfoCdma> cellInfoCdma(mCdma);
  cellInfoCdma.forget(aCdma);
  return NS_OK;
}

NS_IMETHODIMP
nsRilCellInfo::GetLte(nsICellInfoLte** aLte)
{
  RefPtr<nsICellInfoLte> cellInfoLte(mLte);
  cellInfoLte.forget(aLte);
  return NS_OK;
}

NS_IMETHODIMP
nsRilCellInfo::GetWcdma(nsICellInfoWcdma** aWcdma)
{
  RefPtr<nsICellInfoWcdma> cellInfoWcdma(mWcdma);
  cellInfoWcdma.forget(aWcdma);
  return NS_OK;
}

NS_IMETHODIMP
nsRilCellInfo::GetTdscdma(nsICellInfoTdScdma** aTdScdma)
{
  RefPtr<nsICellInfoTdScdma> cellInfoTdScdma(mTdScdma);
  cellInfoTdScdma.forget(aTdScdma);
  return NS_OK;
}

NS_IMETHODIMP
nsRilCellInfo::GetNr(nsICellInfoNr** aNr)
{
  RefPtr<nsICellInfoNr> cellInfoNr(mNr);
  cellInfoNr.forget(aNr);
  return NS_OK;
}

NS_IMETHODIMP
nsRilCellInfo::GetConnectionStatus(int32_t* aConnectionStatus)
{
  *aConnectionStatus = mConnectionStatus;
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsRilCellInfo, nsIRilCellInfo)

/*============================================================================
 *============ Implementation of Class nsHardwareConfig ===================
 *============================================================================*/
/**
 * nsHardwareConfig implementation
 */
nsHardwareConfig::nsHardwareConfig(int32_t aType,
                                   const nsAString& aUuid,
                                   int32_t aState,
                                   nsIHardwareConfigModem* aModem,
                                   nsIHardwareConfigSim* aSim)
  : mType(aType)
  , mUuid(aUuid)
  , mState(aState)
  , mModem(aModem)
  , mSim(aSim)
{
  DEBUG("init nsHardwareConfig");
}

NS_IMETHODIMP
nsHardwareConfig::GetType(int32_t* aType)
{
  *aType = mType;
  return NS_OK;
}

NS_IMETHODIMP
nsHardwareConfig::GetUuid(nsAString& aUuid)
{
  aUuid = mUuid;
  return NS_OK;
}

NS_IMETHODIMP
nsHardwareConfig::GetState(int32_t* aState)
{
  *aState = mState;
  return NS_OK;
}

NS_IMETHODIMP
nsHardwareConfig::GetModem(nsIHardwareConfigModem** aModem)
{
  RefPtr<nsIHardwareConfigModem> hwConfigModem(mModem);
  hwConfigModem.forget(aModem);
  return NS_OK;
}

NS_IMETHODIMP
nsHardwareConfig::GetSim(nsIHardwareConfigSim** aSim)
{
  RefPtr<nsIHardwareConfigSim> hwConfigSim(mSim);
  hwConfigSim.forget(aSim);
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsHardwareConfig, nsIHardwareConfig)

/*============================================================================
 *============ Implementation of Class nsHardwareConfigModem ===================
 *============================================================================*/
/**
 * nsHardwareConfigModem implementation
 */
nsHardwareConfigModem::nsHardwareConfigModem(int32_t aRilModel,
                                             int32_t aRat,
                                             int32_t aMaxVoice,
                                             int32_t aMaxData,
                                             int32_t aMaxStandby)
  : mRilModel(aRilModel)
  , mRat(aRat)
  , mMaxVoice(aMaxVoice)
  , mMaxData(aMaxData)
  , mMaxStandby(aMaxStandby)
{
  DEBUG("init nsHardwareConfigModem");
}

NS_IMETHODIMP
nsHardwareConfigModem::GetRilModel(int32_t* aRilModel)
{
  *aRilModel = mRilModel;
  return NS_OK;
}

NS_IMETHODIMP
nsHardwareConfigModem::GetRat(int32_t* aRat)
{
  *aRat = mRat;
  return NS_OK;
}

NS_IMETHODIMP
nsHardwareConfigModem::GetMaxVoice(int32_t* aMaxVoice)
{
  *aMaxVoice = mMaxVoice;
  return NS_OK;
}
NS_IMETHODIMP
nsHardwareConfigModem::GetMaxData(int32_t* aMaxData)
{
  *aMaxData = mMaxData;
  return NS_OK;
}

NS_IMETHODIMP
nsHardwareConfigModem::GetMaxStandby(int32_t* aMaxStandby)
{
  *aMaxStandby = mMaxStandby;
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsHardwareConfigModem, nsIHardwareConfigModem)

/*============================================================================
 *============ Implementation of Class nsHardwareConfigSim ===================
 *============================================================================*/
/**
 * nsHardwareConfigSim implementation
 */
nsHardwareConfigSim::nsHardwareConfigSim(const nsAString& aModemUuid)
  : mModemUuid(aModemUuid)
{
  DEBUG("init nsHardwareConfigSim");
}

NS_IMETHODIMP
nsHardwareConfigSim::GetModemUuid(nsAString& aModemUuid)
{
  aModemUuid = mModemUuid;
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsHardwareConfigSim, nsIHardwareConfigSim)

/*============================================================================
 *============ Implementation of Class nsRadioCapability ===================
 *============================================================================*/
/**
 * nsRadioCapability implementation
 */
nsRadioCapability::nsRadioCapability(int32_t aSession,
                                     int32_t aPhase,
                                     int32_t aRaf,
                                     const nsAString& aLogicalModemUuid,
                                     int32_t aStatus)
  : mSession(aSession)
  , mPhase(aPhase)
  , mRaf(aRaf)
  , mLogicalModemUuid(aLogicalModemUuid)
  , mStatus(aStatus)
{
  DEBUG("init nsRadioCapability");
}

NS_IMETHODIMP
nsRadioCapability::GetSession(int32_t* aSession)
{
  *aSession = mSession;
  return NS_OK;
}

NS_IMETHODIMP
nsRadioCapability::GetPhase(int32_t* aPhase)
{
  *aPhase = mPhase;
  return NS_OK;
}

NS_IMETHODIMP
nsRadioCapability::GetRaf(int32_t* aRaf)
{
  *aRaf = mRaf;
  return NS_OK;
}

NS_IMETHODIMP
nsRadioCapability::GetLogicalModemUuid(nsAString& aLogicalModemUuid)
{
  aLogicalModemUuid = mLogicalModemUuid;
  return NS_OK;
}

NS_IMETHODIMP
nsRadioCapability::GetStatus(int32_t* aStatus)
{
  *aStatus = mStatus;
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsRadioCapability, nsIRadioCapability)

/*============================================================================
 *============ Implementation of Class nsLceStatusInfo ===================
 *============================================================================*/
/**
 * nsLceStatusInfo implementation
 */
nsLceStatusInfo::nsLceStatusInfo(int32_t aLceStatus, int32_t aActualIntervalMs)
  : mLceStatus(aLceStatus)
  , mActualIntervalMs(aActualIntervalMs)
{
  DEBUG("init nsLceStatusInfo");
}

NS_IMETHODIMP
nsLceStatusInfo::GetLceStatus(int32_t* aLceStatus)
{
  *aLceStatus = mLceStatus;
  return NS_OK;
}

NS_IMETHODIMP
nsLceStatusInfo::GetActualIntervalMs(int32_t* aActualIntervalMs)
{
  *aActualIntervalMs = mActualIntervalMs;
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsLceStatusInfo, nsILceStatusInfo)

/*============================================================================
 *============ Implementation of Class nsLceDataInfo ===================
 *============================================================================*/
/**
 * nsLceDataInfo implementation
 */
nsLceDataInfo::nsLceDataInfo(int32_t aLastHopCapacityKbps,
                             int32_t aConfidenceLevel,
                             bool aLceSuspended)
  : mLastHopCapacityKbps(aLastHopCapacityKbps)
  , mConfidenceLevel(aConfidenceLevel)
  , mLceSuspended(aLceSuspended)
{
  DEBUG("init nsLceDataInfo");
}

NS_IMETHODIMP
nsLceDataInfo::GetLastHopCapacityKbps(int32_t* aLastHopCapacityKbps)
{
  *aLastHopCapacityKbps = mLastHopCapacityKbps;
  return NS_OK;
}

NS_IMETHODIMP
nsLceDataInfo::GetConfidenceLevel(int32_t* aConfidenceLevel)
{
  *aConfidenceLevel = mConfidenceLevel;
  return NS_OK;
}

NS_IMETHODIMP
nsLceDataInfo::GetLceSuspended(bool* aLceSuspended)
{
  *aLceSuspended = mLceSuspended;
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsLceDataInfo, nsILceDataInfo)

/*============================================================================
 *============ Implementation of Class nsPcoDataInfo ===================
 *============================================================================*/
/**
 * nsPcoDataInfo implementation
 */
nsPcoDataInfo::nsPcoDataInfo(int32_t aCid,
                             const nsAString& aBearerProto,
                             bool aPcoId,
                             nsTArray<int32_t>& aContents)
  : mCid(aCid)
  , mBearerProto(aBearerProto)
  , mPcoId(aPcoId)
  , mContents(aContents.Clone())
{
  DEBUG("init nsPcoDataInfo");
}

NS_IMETHODIMP
nsPcoDataInfo::GetCid(int32_t* aCid)
{
  *aCid = mCid;
  return NS_OK;
}

NS_IMETHODIMP
nsPcoDataInfo::GetBearerProto(nsAString& aBearerProto)
{
  aBearerProto = mBearerProto;
  return NS_OK;
}

NS_IMETHODIMP
nsPcoDataInfo::GetPcoId(int32_t* aPcoId)
{
  *aPcoId = mPcoId;
  return NS_OK;
}

NS_IMETHODIMP
nsPcoDataInfo::GetContents(uint32_t* count, int32_t** contents)
{
  *count = mContents.Length();
  *contents = (int32_t*)moz_xmalloc((*count) * sizeof(int32_t));
  NS_ENSURE_TRUE(*contents, NS_ERROR_OUT_OF_MEMORY);

  for (uint32_t i = 0; i < *count; i++) {
    (*contents)[i] = mContents[i];
  }

  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsPcoDataInfo, nsIPcoDataInfo)

/*============================================================================
 *============ Implementation of Class nsAppStatus ===================
 *============================================================================*/
/**
 * nsAppStatus implementation
 */
nsAppStatus::nsAppStatus(int32_t aAppType,
                         int32_t aAppState,
                         int32_t aPersoSubstate,
                         const nsAString& aAidPtr,
                         const nsAString& aAppLabelPtr,
                         int32_t aPin1Replaced,
                         int32_t aPin1,
                         int32_t aPin2)
  : mAppType(aAppType)
  , mAppState(aAppState)
  , mPersoSubstate(aPersoSubstate)
  , mAidPtr(aAidPtr)
  , mAppLabelPtr(aAppLabelPtr)
  , mPin1Replaced(aPin1Replaced)
  , mPin1(aPin1)
  , mPin2(aPin2)
{
  DEBUG("init nsAppStatus");
}

NS_IMETHODIMP
nsAppStatus::GetAppType(int32_t* aAppType)
{
  *aAppType = mAppType;
  return NS_OK;
}

NS_IMETHODIMP
nsAppStatus::GetAppState(int32_t* aAppState)
{
  *aAppState = mAppState;
  return NS_OK;
}

NS_IMETHODIMP
nsAppStatus::GetPersoSubstate(int32_t* aPersoSubstate)
{
  *aPersoSubstate = mPersoSubstate;
  return NS_OK;
}

NS_IMETHODIMP
nsAppStatus::GetAidPtr(nsAString& aAidPtr)
{
  aAidPtr = mAidPtr;
  return NS_OK;
}

NS_IMETHODIMP
nsAppStatus::GetAppLabelPtr(nsAString& aAppLabelPtr)
{
  aAppLabelPtr = mAppLabelPtr;
  return NS_OK;
}

NS_IMETHODIMP
nsAppStatus::GetPin1Replaced(int32_t* aPin1Replaced)
{
  *aPin1Replaced = mPin1Replaced;
  return NS_OK;
}

NS_IMETHODIMP
nsAppStatus::GetPin1(int32_t* aPin1)
{
  *aPin1 = mPin1;
  return NS_OK;
}

NS_IMETHODIMP
nsAppStatus::GetPin2(int32_t* aPin2)
{
  *aPin2 = mPin2;
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsAppStatus, nsIAppStatus)

/*============================================================================
 *============ Implementation of Class nsCardStatus ===================
 *============================================================================*/
/**
 * nsCardStatus implementation
 */
nsCardStatus::nsCardStatus(int32_t aCardState,
                           int32_t aUniversalPinState,
                           int32_t aGsmUmtsSubscriptionAppIndex,
                           int32_t aCdmaSubscriptionAppIndex,
                           int32_t aImsSubscriptionAppIndex,
                           nsTArray<RefPtr<nsAppStatus>>& aApplications,
                           int32_t aPhysicalSlotId,
                           const nsAString& aAtr,
                           const nsAString& aIccid,
                           const nsAString& aEid)
  : mCardState(aCardState)
  , mUniversalPinState(aUniversalPinState)
  , mGsmUmtsSubscriptionAppIndex(aGsmUmtsSubscriptionAppIndex)
  , mCdmaSubscriptionAppIndex(aCdmaSubscriptionAppIndex)
  , mImsSubscriptionAppIndex(aImsSubscriptionAppIndex)
  , mApplications(aApplications.Clone())
  , mPhysicalSlotId(aPhysicalSlotId)
  , mAtr(aAtr)
  , mIccid(aIccid)
  , mEid(aEid)
{
  DEBUG("init nsCardStatus");
}

NS_IMETHODIMP
nsCardStatus::GetCardState(int32_t* aCardState)
{
  *aCardState = mCardState;
  return NS_OK;
}

NS_IMETHODIMP
nsCardStatus::GetUniversalPinState(int32_t* aUniversalPinState)
{
  *aUniversalPinState = mUniversalPinState;
  return NS_OK;
}

NS_IMETHODIMP
nsCardStatus::GetGsmUmtsSubscriptionAppIndex(
  int32_t* aGsmUmtsSubscriptionAppIndex)
{
  *aGsmUmtsSubscriptionAppIndex = mGsmUmtsSubscriptionAppIndex;
  return NS_OK;
}

NS_IMETHODIMP
nsCardStatus::GetCdmaSubscriptionAppIndex(int32_t* aCdmaSubscriptionAppIndex)
{
  *aCdmaSubscriptionAppIndex = mCdmaSubscriptionAppIndex;
  return NS_OK;
}

NS_IMETHODIMP
nsCardStatus::GetImsSubscriptionAppIndex(int32_t* aImsSubscriptionAppIndex)
{
  *aImsSubscriptionAppIndex = mImsSubscriptionAppIndex;
  return NS_OK;
}

NS_IMETHODIMP
nsCardStatus::GetAppStatus(uint32_t* count, nsIAppStatus*** applications)
{
  // Allocate prefix arrays
  *count = mApplications.Length();
  nsIAppStatus** application =
    (nsIAppStatus**)moz_xmalloc(*count * sizeof(nsIAppStatus*));

  for (uint32_t i = 0; i < *count; i++) {
    NS_ADDREF(application[i] = mApplications[i]);
  }

  *applications = application;

  return NS_OK;
}

NS_IMETHODIMP
nsCardStatus::GetPhysicalSlotId(int32_t* aPhysicalSlotId)
{
  *aPhysicalSlotId = mPhysicalSlotId;
  return NS_OK;
}

NS_IMETHODIMP
nsCardStatus::GetAtr(nsAString& aAtr)
{
  aAtr = mAtr;
  return NS_OK;
}

NS_IMETHODIMP
nsCardStatus::GetIccid(nsAString& aIccid)
{
  aIccid = mIccid;
  return NS_OK;
}

NS_IMETHODIMP
nsCardStatus::GetEid(nsAString& aEid)
{
  aEid = mEid;
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsCardStatus, nsICardStatus)

/*============================================================================
 *============ Implementation of Class nsVoiceRegState ===================
 *============================================================================*/
/**
 * nsVoiceRegState implementation
 */
nsVoiceRegState::nsVoiceRegState(int32_t aRegState,
                                 int32_t aRat,
                                 bool aCssSupported,
                                 int32_t aRoamingIndicator,
                                 int32_t aSystemIsInPrl,
                                 int32_t aDefaultRoamingIndicator,
                                 int32_t aReasonForDenial,
                                 nsCellIdentity* aCellIdentity,
                                 const nsAString& aRegisteredPlmn)
  : mRegState(aRegState)
  , mRat(aRat)
  , mCssSupported(aCssSupported)
  , mRoamingIndicator(aRoamingIndicator)
  , mSystemIsInPrl(aSystemIsInPrl)
  , mDefaultRoamingIndicator(aDefaultRoamingIndicator)
  , mReasonForDenial(aReasonForDenial)
  , mCellIdentity(aCellIdentity)
  , mRegisteredPlmn(aRegisteredPlmn)
{
  DEBUG("init nsVoiceRegState");
}

NS_IMETHODIMP
nsVoiceRegState::GetRegState(int32_t* aRegState)
{
  *aRegState = mRegState;
  return NS_OK;
}

NS_IMETHODIMP
nsVoiceRegState::GetRat(int32_t* aRat)
{
  *aRat = mRat;
  return NS_OK;
}

NS_IMETHODIMP
nsVoiceRegState::GetCssSupported(bool* aCssSupported)
{
  *aCssSupported = mCssSupported;
  return NS_OK;
}

NS_IMETHODIMP
nsVoiceRegState::GetRoamingIndicator(int32_t* aRoamingIndicator)
{
  *aRoamingIndicator = mRoamingIndicator;
  return NS_OK;
}

NS_IMETHODIMP
nsVoiceRegState::GetSystemIsInPrl(int32_t* aSystemIsInPrl)
{
  *aSystemIsInPrl = mSystemIsInPrl;
  return NS_OK;
}

NS_IMETHODIMP
nsVoiceRegState::GetDefaultRoamingIndicator(int32_t* aDefaultRoamingIndicator)
{
  *aDefaultRoamingIndicator = mDefaultRoamingIndicator;
  return NS_OK;
}

NS_IMETHODIMP
nsVoiceRegState::GetReasonForDenial(int32_t* aReasonForDenial)
{
  *aReasonForDenial = mReasonForDenial;
  return NS_OK;
}

NS_IMETHODIMP
nsVoiceRegState::GetCellIdentity(nsICellIdentity** aCellIdentity)
{
  RefPtr<nsICellIdentity> cellIdentity(mCellIdentity);
  cellIdentity.forget(aCellIdentity);
  return NS_OK;
}

NS_IMETHODIMP
nsVoiceRegState::GetRegisteredPlmn(nsAString& aRegisteredPlmn)
{
  aRegisteredPlmn = mRegisteredPlmn;
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsVoiceRegState, nsIVoiceRegState)

/*============================================================================
 *============ Implementation of Class nsDataRegState ===================
 *============================================================================*/
nsNrIndicators::nsNrIndicators(bool isEndcAvailable,
                               bool isDcNrRestricted,
                               bool isNrAvailable)
  : mIsEndcAvailable(isEndcAvailable)
  , mIsDcNrRestricted(isDcNrRestricted)
  , mIsNrAvailable(isNrAvailable)
{
  DEBUG("init nsNrIndicators");
}

NS_IMETHODIMP
nsNrIndicators::GetIsEndcAvailable(bool* isEndcAvailable)
{
  *isEndcAvailable = mIsEndcAvailable;
  return NS_OK;
}

NS_IMETHODIMP
nsNrIndicators::GetIsNrAvailable(bool* isNrAvailable)
{
  *isNrAvailable = mIsNrAvailable;
  return NS_OK;
}

NS_IMETHODIMP
nsNrIndicators::GetIsDcNrRestricted(bool* isDcNrRestricted)
{
  *isDcNrRestricted = mIsDcNrRestricted;
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsNrIndicators, nsINrIndicators)

nsLteVopsInfo::nsLteVopsInfo(bool isVopsSupported, bool isEmcBearerSupported)
  : mIsVopsSupported(isVopsSupported)
  , mIsEmcBearerSupported(isEmcBearerSupported)
{
  DEBUG("init nsLteVopsInfo");
}
NS_IMETHODIMP
nsLteVopsInfo::GetIsVopsSupported(bool* isVopsSupported)
{
  *isVopsSupported = mIsVopsSupported;
  return NS_OK;
}
NS_IMETHODIMP
nsLteVopsInfo::GetIsEmcBearerSupported(bool* isEmcBearerSupported)
{
  *isEmcBearerSupported = mIsEmcBearerSupported;
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsLteVopsInfo, nsILteVopsInfo)

/*============================================================================
 *============ Implementation of Class nsNrVopsInfo ===================
 *============================================================================*/
nsNrVopsInfo::nsNrVopsInfo(int32_t aVopsSupported,
                           int32_t aEmcSupported,
                           int32_t aEmfSupported)
  : mVopsSupported(aVopsSupported)
  , mEmcSupported(aEmcSupported)
  , mEmfSupported(aEmfSupported)
{
  DEBUG("init nsNrVopsInfo");
}
NS_IMETHODIMP
nsNrVopsInfo::GetVopsSupported(int32_t* aVopsSupported)
{
  *aVopsSupported = mVopsSupported;
  return NS_OK;
}
NS_IMETHODIMP
nsNrVopsInfo::GetEmcSupported(int32_t* aEmcSupported)
{
  *aEmcSupported = mEmcSupported;
  return NS_OK;
}
NS_IMETHODIMP
nsNrVopsInfo::GetEmfSupported(int32_t* aEmfSupported)
{
  *aEmfSupported = mEmfSupported;
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsNrVopsInfo, nsINrVopsInfo)

/**
 * nsDataRegState implementation
 */
nsDataRegState::nsDataRegState(int32_t aRegState,
                               int32_t aRat,
                               int32_t aReasonDataDenied,
                               int32_t aMaxDataCalls,
                               nsCellIdentity* aCellIdentity,
                               nsLteVopsInfo* aVopsInfo,
                               nsNrIndicators* aNrIndicators,
                               const nsAString& aRegisteredPlmn,
                               nsNrVopsInfo* aNrVopsInfo)
  : mRegState(aRegState)
  , mRat(aRat)
  , mReasonDataDenied(aReasonDataDenied)
  , mMaxDataCalls(aMaxDataCalls)
  , mCellIdentity(aCellIdentity)
  , mVopsInfo(aVopsInfo)
  , mNrIndicators(aNrIndicators)
  , mRegisteredPlmn(aRegisteredPlmn)
  , mNrVopsInfo(aNrVopsInfo)
{
  DEBUG("init nsDataRegState");
}

NS_IMETHODIMP
nsDataRegState::GetRegState(int32_t* aRegState)
{
  *aRegState = mRegState;
  return NS_OK;
}

NS_IMETHODIMP
nsDataRegState::GetRat(int32_t* aRat)
{
  *aRat = mRat;
  return NS_OK;
}

NS_IMETHODIMP
nsDataRegState::GetReasonDataDenied(int32_t* aReasonDataDenied)
{
  *aReasonDataDenied = mReasonDataDenied;
  return NS_OK;
}

NS_IMETHODIMP
nsDataRegState::GetMaxDataCalls(int32_t* aMaxDataCalls)
{
  *aMaxDataCalls = mMaxDataCalls;
  return NS_OK;
}

NS_IMETHODIMP
nsDataRegState::GetCellIdentity(nsICellIdentity** aCellIdentity)
{
  RefPtr<nsICellIdentity> cellIdentity(mCellIdentity);
  cellIdentity.forget(aCellIdentity);
  return NS_OK;
}

NS_IMETHODIMP
nsDataRegState::GetVopsInfo(nsILteVopsInfo** aVopsInfo)
{
  RefPtr<nsILteVopsInfo> vopsInfo(mVopsInfo);
  vopsInfo.forget(aVopsInfo);
  return NS_OK;
}

NS_IMETHODIMP
nsDataRegState::GetNrIndicators(nsINrIndicators** aNrIndicators)
{
  RefPtr<nsINrIndicators> nrIndicators(mNrIndicators);
  nrIndicators.forget(aNrIndicators);
  return NS_OK;
}

NS_IMETHODIMP
nsDataRegState::GetRegisteredPlmn(nsAString& aRegisteredPlmn)
{
  aRegisteredPlmn = mRegisteredPlmn;
  return NS_OK;
}

NS_IMETHODIMP
nsDataRegState::GetNrVopsInfo(nsINrVopsInfo** aNrVopsInfo)
{
  RefPtr<nsINrVopsInfo> nrVopsInfo(mNrVopsInfo);
  nrVopsInfo.forget(aNrVopsInfo);
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsDataRegState, nsIDataRegState)

/*============================================================================
 *============ Implementation of Class nsOperatorInfo ===================
 *============================================================================*/
/**
 * nsOperatorInfo implementation
 */
nsOperatorInfo::nsOperatorInfo(const nsAString& aAlphaLong,
                               const nsAString& aAlphaShort,
                               const nsAString& aOperatorNumeric,
                               int32_t aStatus)
  : mAlphaLong(aAlphaLong)
  , mAlphaShort(aAlphaShort)
  , mOperatorNumeric(aOperatorNumeric)
  , mStatus(aStatus)
{
  DEBUG("init nsOperatorInfo");
}

NS_IMETHODIMP
nsOperatorInfo::GetAlphaLong(nsAString& aAlphaLong)
{
  aAlphaLong = mAlphaLong;
  return NS_OK;
}

NS_IMETHODIMP
nsOperatorInfo::GetAlphaShort(nsAString& aAlphaShort)
{
  aAlphaShort = mAlphaShort;
  return NS_OK;
}

NS_IMETHODIMP
nsOperatorInfo::GetOperatorNumeric(nsAString& aOperatorNumeric)
{
  aOperatorNumeric = mOperatorNumeric;
  return NS_OK;
}

NS_IMETHODIMP
nsOperatorInfo::GetStatus(int32_t* aStatus)
{
  *aStatus = mStatus;
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsOperatorInfo, nsIOperatorInfo)

/*============================================================================
 *============ Implementation of Class nsNeighboringCell ===================
 *============================================================================*/
/**
 * nsNeighboringCell implementation
 */
nsNeighboringCell::nsNeighboringCell(const nsAString& aCid, int32_t aRssi)
  : mCid(aCid)
  , mRssi(aRssi)
{
  DEBUG("init nsNeighboringCell");
}

NS_IMETHODIMP
nsNeighboringCell::GetCid(nsAString& aCid)
{
  aCid = mCid;
  return NS_OK;
}

NS_IMETHODIMP
nsNeighboringCell::GetRssi(int32_t* aRssi)
{
  *aRssi = mRssi;
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsNeighboringCell, nsINeighboringCell)

/*============================================================================
 *============ Implementation of Class nsCall ===================
 *============================================================================*/
/**
 * nsCall implementation
 */
nsCall::nsCall(int32_t aState,
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
               const nsAString& aForwardedNumber)
  : mState(aState)
  , mIndex(aIndex)
  , mToa(aToa)
  , mIsMpty(aIsMpty)
  , mIsMT(aIsMT)
  , mAls(aAls)
  , mIsVoice(aIsVoice)
  , mIsVoicePrivacy(aIsVoicePrivacy)
  , mNumber(aNumber)
  , mNumberPresentation(aNumberPresentation)
  , mName(aName)
  , mNamePresentation(aNamePresentation)
  , mUusInfo(aUusInfo.Clone())
  , mAudioQuality(aAudioQuality)
  , mForwardedNumber(aForwardedNumber)
{
  DEBUG("init nsCall");
}

NS_IMETHODIMP
nsCall::GetState(int32_t* aState)
{
  *aState = mState;
  return NS_OK;
}

NS_IMETHODIMP
nsCall::GetIndex(int32_t* aIndex)
{
  *aIndex = mIndex;
  return NS_OK;
}

NS_IMETHODIMP
nsCall::GetToa(int32_t* aToa)
{
  *aToa = mToa;
  return NS_OK;
}

NS_IMETHODIMP
nsCall::GetIsMpty(bool* aIsMpty)
{
  *aIsMpty = mIsMpty;
  return NS_OK;
}

NS_IMETHODIMP
nsCall::GetIsMT(bool* aIsMT)
{
  *aIsMT = mIsMT;
  return NS_OK;
}

NS_IMETHODIMP
nsCall::GetAls(int32_t* aAls)
{
  *aAls = mAls;
  return NS_OK;
}

NS_IMETHODIMP
nsCall::GetIsVoice(bool* aIsVoice)
{
  *aIsVoice = mIsVoice;
  return NS_OK;
}

NS_IMETHODIMP
nsCall::GetIsVoicePrivacy(bool* aIsVoicePrivacy)
{
  *aIsVoicePrivacy = mIsVoicePrivacy;
  return NS_OK;
}

NS_IMETHODIMP
nsCall::GetNumber(nsAString& aNumber)
{
  aNumber = mNumber;
  return NS_OK;
}

NS_IMETHODIMP
nsCall::GetNumberPresentation(int32_t* aNumberPresentation)
{
  *aNumberPresentation = mNumberPresentation;
  return NS_OK;
}

NS_IMETHODIMP
nsCall::GetName(nsAString& aName)
{
  aName = mName;
  return NS_OK;
}

NS_IMETHODIMP
nsCall::GetNamePresentation(int32_t* aNamePresentation)
{
  *aNamePresentation = mNamePresentation;
  return NS_OK;
}

NS_IMETHODIMP
nsCall::GetUusInfo(uint32_t* count, nsIUusInfo*** uusInfos)
{
  *count = mUusInfo.Length();
  nsIUusInfo** uusinfo =
    (nsIUusInfo**)moz_xmalloc(*count * sizeof(nsIUusInfo*));

  for (uint32_t i = 0; i < *count; i++) {
    NS_ADDREF(uusinfo[i] = mUusInfo[i]);
  }

  *uusInfos = uusinfo;
  return NS_OK;
}

NS_IMETHODIMP
nsCall::GetAudioQuality(int32_t* aAudioQuality)
{
  *aAudioQuality = mAudioQuality;
  return NS_OK;
}

NS_IMETHODIMP
nsCall::GetForwardedNumber(nsAString& aForwardedNumber)
{
  aForwardedNumber = mForwardedNumber;
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsCall, nsICall)

/*============================================================================
 *============ Implementation of Class nsUusInfo ===================
 *============================================================================*/
/**
 * nsUusInfo implementation
 */
nsUusInfo::nsUusInfo(int32_t aUusType,
                     int32_t aUusDcs,
                     const nsAString& aUusData)
  : mUusType(aUusType)
  , mUusDcs(aUusDcs)
  , mUusData(aUusData)
{
  DEBUG("init nsUusInfo");
}

NS_IMETHODIMP
nsUusInfo::GetUusType(int32_t* aUusType)
{
  *aUusType = mUusType;
  return NS_OK;
}

NS_IMETHODIMP
nsUusInfo::GetUusDcs(int32_t* aUusDcs)
{
  *aUusDcs = mUusDcs;
  return NS_OK;
}

NS_IMETHODIMP
nsUusInfo::GetUusData(nsAString& aUusData)
{
  aUusData = mUusData;
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsUusInfo, nsIUusInfo)

/*============================================================================
 *============ Implementation of Class nsIccIoResult ===================
 *============================================================================*/
/**
 * nsIccIoResult implementation
 */
nsIccIoResult::nsIccIoResult(int32_t aSw1,
                             int32_t aSw2,
                             const nsAString& aSimResponse)
  : mSw1(aSw1)
  , mSw2(aSw2)
  , mSimResponse(aSimResponse)
{
  DEBUG("init nsIccIoResult");
}

NS_IMETHODIMP
nsIccIoResult::GetSw1(int32_t* aSw1)
{
  *aSw1 = mSw1;
  return NS_OK;
}

NS_IMETHODIMP
nsIccIoResult::GetSw2(int32_t* aSw2)
{
  *aSw2 = mSw2;
  return NS_OK;
}

NS_IMETHODIMP
nsIccIoResult::GetSimResponse(nsAString& aSimResponse)
{
  aSimResponse = mSimResponse;
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsIccIoResult, nsIIccIoResult)

/*============================================================================
 *============ Implementation of Class nsCallForwardInfo ===================
 *============================================================================*/
/**
 * nsCallForwardInfo implementation
 */
nsCallForwardInfo::nsCallForwardInfo(int32_t aStatus,
                                     int32_t aReason,
                                     int32_t aServiceClass,
                                     int32_t aToa,
                                     const nsAString& aNumber,
                                     int32_t aTimeSeconds)
  : mStatus(aStatus)
  , mReason(aReason)
  , mServiceClass(aServiceClass)
  , mToa(aToa)
  , mNumber(aNumber)
  , mTimeSeconds(aTimeSeconds)
{
  DEBUG("init nsCallForwardInfo");
}

NS_IMETHODIMP
nsCallForwardInfo::GetStatus(int32_t* aStatus)
{
  *aStatus = mStatus;
  return NS_OK;
}

NS_IMETHODIMP
nsCallForwardInfo::GetReason(int32_t* aReason)
{
  *aReason = mReason;
  return NS_OK;
}

NS_IMETHODIMP
nsCallForwardInfo::GetServiceClass(int32_t* aServiceClass)
{
  *aServiceClass = mServiceClass;
  return NS_OK;
}

NS_IMETHODIMP
nsCallForwardInfo::GetToa(int32_t* aToa)
{
  *aToa = mToa;
  return NS_OK;
}

NS_IMETHODIMP
nsCallForwardInfo::GetNumber(nsAString& aNumber)
{
  aNumber = mNumber;
  return NS_OK;
}

NS_IMETHODIMP
nsCallForwardInfo::GetTimeSeconds(int32_t* aTimeSeconds)
{
  *aTimeSeconds = mTimeSeconds;
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsCallForwardInfo, nsICallForwardInfo)

/*============================================================================
 *============ Implementation of Class nsSendSmsResult ===================
 *============================================================================*/
/**
 * nsSendSmsResult implementation
 */
nsSendSmsResult::nsSendSmsResult(int32_t aMessageRef,
                                 const nsAString& aAckPDU,
                                 int32_t aErrorCode)
  : mMessageRef(aMessageRef)
  , mAckPDU(aAckPDU)
  , mErrorCode(aErrorCode)
{
  DEBUG("init nsSendSmsResult");
}

NS_IMETHODIMP
nsSendSmsResult::GetMessageRef(int32_t* aMessageRef)
{
  *aMessageRef = mMessageRef;
  return NS_OK;
}

NS_IMETHODIMP
nsSendSmsResult::GetAckPDU(nsAString& aAckPDU)
{
  aAckPDU = mAckPDU;
  return NS_OK;
}

NS_IMETHODIMP
nsSendSmsResult::GetErrorCode(int32_t* aErrorCode)
{
  *aErrorCode = mErrorCode;
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsSendSmsResult, nsISendSmsResult)

/*============================================================================
 *============ Implementation of Class nsILinkCapacityEstimate
 *===================
 *============================================================================*/
/**
 * nsLinkCapacityEstimate implementation
 */
nsLinkCapacityEstimate::nsLinkCapacityEstimate(
  int32_t aDownlinkCapacityKbps,
  int32_t aUplinkCapacityKbps,
  uint32_t aSecondaryDownlinkCapacityKbps,
  uint32_t aSecondaryUplinkCapacityKbps)
  : mDownlinkCapacityKbps(aDownlinkCapacityKbps)
  , mUplinkCapacityKbps(aUplinkCapacityKbps)
  , mSecondaryDownlinkCapacityKbps(aSecondaryDownlinkCapacityKbps)
  , mSecondaryUplinkCapacityKbps(aSecondaryUplinkCapacityKbps)
{
  DEBUG("init nsLinkCapacityEstimate");
}

NS_IMETHODIMP
nsLinkCapacityEstimate::GetDownlinkCapacityKbps(int32_t* aDownlinkCapacityKbps)
{
  *aDownlinkCapacityKbps = mDownlinkCapacityKbps;
  return NS_OK;
}

NS_IMETHODIMP
nsLinkCapacityEstimate::GetUplinkCapacityKbps(int32_t* aUplinkCapacityKbps)
{
  *aUplinkCapacityKbps = mUplinkCapacityKbps;
  return NS_OK;
}

NS_IMETHODIMP
nsLinkCapacityEstimate::GetSecondaryDownlinkCapacityKbps(
  uint32_t* aSecondaryDownlinkCapacityKbps)
{
  *aSecondaryDownlinkCapacityKbps = mSecondaryDownlinkCapacityKbps;
  return NS_OK;
}

NS_IMETHODIMP
nsLinkCapacityEstimate::GetSecondaryUplinkCapacityKbps(
  uint32_t* aSecondaryUplinkCapacityKbps)
{
  *aSecondaryUplinkCapacityKbps = mSecondaryUplinkCapacityKbps;
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsLinkCapacityEstimate, nsILinkCapacityEstimate)

/*============================================================================
 *============ Implementation of Class nsIPhysicalChannelConfig
 *===================
 *============================================================================*/
/**
 * nsPhysicalChannelConfig implementation
 */
nsPhysicalChannelConfig::nsPhysicalChannelConfig(
  const int32_t status,
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
  int32_t aNgranBand)
  : mStatus(status)
  , mCellBandwidthDownlink(cellBandwidthDownlink)
  , mRat(rat)
  , mDiscriminator(discriminator)
  , mFrequencyRange(frequencyRange)
  , mChannelNumber(channelNumber)
  , mContextIds(contextIds.Clone())
  , mPhysicalCellId(physicalCellId)
  , mDownlinkChannelNumber(aDownlinkChannelNumber)
  , mUplinkChannelNumber(aUplinkChannelNumber)
  , mCellBandwidthUplinkKhz(aCellBandwidthUplinkKhz)
  , mRan_discriminator(aRan_discriminator)
  , mGeranBand(aGeranBand)
  , mUtranBand(aUtranBand)
  , mEutranBand(aEutranBand)
  , mNgranBand(aNgranBand)
{
  DEBUG("init nsPhysicalChannelConfig");
}

NS_IMETHODIMP
nsPhysicalChannelConfig::GetStatus(int32_t* aStatus)
{
  *aStatus = mStatus;
  return NS_OK;
}

NS_IMETHODIMP
nsPhysicalChannelConfig::GetCellBandwidthDownlink(
  uint32_t* aCellBandwidthDownlink)
{
  *aCellBandwidthDownlink = mCellBandwidthDownlink;
  return NS_OK;
}

NS_IMETHODIMP
nsPhysicalChannelConfig::GetRat(int32_t* aRat)
{
  *aRat = mRat;
  return NS_OK;
}

NS_IMETHODIMP
nsPhysicalChannelConfig::GetDiscriminator(uint8_t* aDiscriminator)
{
  *aDiscriminator = mDiscriminator;
  return NS_OK;
}

NS_IMETHODIMP
nsPhysicalChannelConfig::GetFrequencyRange(int32_t* aFrequencyRange)
{
  *aFrequencyRange = mFrequencyRange;
  return NS_OK;
}

NS_IMETHODIMP
nsPhysicalChannelConfig::GetChannelNumber(int32_t* channelNumber)
{
  *channelNumber = mChannelNumber;
  return NS_OK;
}

NS_IMETHODIMP
nsPhysicalChannelConfig::GetContextIds(nsTArray<int32_t>& aContextIds)
{
  aContextIds = mContextIds.Clone();
  return NS_OK;
}

NS_IMETHODIMP
nsPhysicalChannelConfig::GetPhysicalCellId(uint32_t* aPhysicalCellId)
{
  *aPhysicalCellId = mPhysicalCellId;
  return NS_OK;
}

NS_IMETHODIMP
nsPhysicalChannelConfig::GetDownlinkChannelNumber(
  int32_t* aDownlinkChannelNumber)
{
  *aDownlinkChannelNumber = mDownlinkChannelNumber;
  return NS_OK;
}

NS_IMETHODIMP
nsPhysicalChannelConfig::GetUplinkChannelNumber(int32_t* aUplinkChannelNumber)
{
  *aUplinkChannelNumber = mUplinkChannelNumber;
  return NS_OK;
}

NS_IMETHODIMP
nsPhysicalChannelConfig::GetCellBandwidthUplinkKhz(
  int32_t* aCellBandwidthUplinkKhz)
{
  *aCellBandwidthUplinkKhz = mCellBandwidthUplinkKhz;
  return NS_OK;
}

NS_IMETHODIMP
nsPhysicalChannelConfig::GetRan_discriminator(int32_t* aRan_discriminator)
{
  *aRan_discriminator = mRan_discriminator;
  return NS_OK;
}

NS_IMETHODIMP
nsPhysicalChannelConfig::GetGeranBand(int32_t* aGeranBand)
{
  *aGeranBand = mGeranBand;
  return NS_OK;
}

NS_IMETHODIMP
nsPhysicalChannelConfig::GetUtranBand(int32_t* aUtranBand)
{
  *aUtranBand = mUtranBand;
  return NS_OK;
}

NS_IMETHODIMP
nsPhysicalChannelConfig::GetEutranBand(int32_t* aEutranBand)
{
  *aEutranBand = mEutranBand;
  return NS_OK;
}

NS_IMETHODIMP
nsPhysicalChannelConfig::GetNgranBand(int32_t* aNgranBand)
{
  *aNgranBand = mNgranBand;
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsPhysicalChannelConfig, nsIPhysicalChannelConfig)

nsAllowedCarriers::nsAllowedCarriers(nsCarrierRestrictionsWithPriority* crp,
                                     const int32_t simLockMultiSimPolicy)
  : mCrp(crp)
  , mSimLockMultiSimPolicy(simLockMultiSimPolicy)
{
  DEBUG("init nsAllowedCarriers");
}

NS_IMETHODIMP
nsAllowedCarriers::GetCrp(nsICarrierRestrictionsWithPriority** aCrp)
{
  RefPtr<nsCarrierRestrictionsWithPriority> crp(mCrp);
  crp.forget(aCrp);
  return NS_OK;
}

NS_IMETHODIMP
nsAllowedCarriers::GetSimLockMultiSimPolicy(int32_t* simLockMultiSimPolicy)
{
  *simLockMultiSimPolicy = mSimLockMultiSimPolicy;
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsAllowedCarriers, nsIAllowedCarriers)

nsEmergencyNumber::nsEmergencyNumber(const nsAString& number,
                                     const nsAString& mcc,
                                     const nsAString& mnc,
                                     uint32_t categories,
                                     nsTArray<nsString>& urns,
                                     uint32_t sources)
  : mNumber(number)
  , mMcc(mcc)
  , mMnc(mnc)
  , mCategories(categories)
  , mSources(sources)
{
  for (uint32_t i = 0; i < urns.Length(); i++) {
    mUrns.AppendElement(urns[i]);
  }
}

NS_IMETHODIMP
nsEmergencyNumber::GetNumber(nsAString& number)
{
  number = mNumber;
  return NS_OK;
}
NS_IMETHODIMP
nsEmergencyNumber::GetMcc(nsAString& mcc)
{
  mcc = mMcc;
  return NS_OK;
}
NS_IMETHODIMP
nsEmergencyNumber::GetMnc(nsAString& mnc)
{
  mnc = mMnc;
  return NS_OK;
}
NS_IMETHODIMP
nsEmergencyNumber::GetCategories(uint32_t* categories)
{
  *categories = mCategories;
  return NS_OK;
}
NS_IMETHODIMP
nsEmergencyNumber::GetUrns(nsTArray<nsString>& urns)
{
  urns = mUrns.Clone();
  return NS_OK;
}
NS_IMETHODIMP
nsEmergencyNumber::GetSources(uint32_t* sources)
{
  *sources = mSources;
  return NS_OK;
}
NS_IMPL_ISUPPORTS(nsEmergencyNumber, nsIEmergencyNumber)

nsNetworkScanResult::nsNetworkScanResult(
  const int32_t status,
  const int32_t error,
  nsTArray<RefPtr<nsRilCellInfo>>& networkInfos)
  : mStatus(status)
  , mError(error)
  , mNetworkInfos(networkInfos.Clone())
{
  DEBUG("init nsNetworkScanResult");
}

NS_IMETHODIMP
nsNetworkScanResult::GetStatus(int32_t* status)
{
  *status = mStatus;
  return NS_OK;
}

NS_IMETHODIMP
nsNetworkScanResult::GetError(int32_t* error)
{
  *error = mError;
  return NS_OK;
}

NS_IMETHODIMP
nsNetworkScanResult::GetNetworkInfos(
  nsTArray<RefPtr<nsIRilCellInfo>>& aNetworkInfos)
{
  for (uint32_t i = 0; i < mNetworkInfos.Length(); i++) {
    aNetworkInfos.AppendElement(mNetworkInfos[i]);
  }
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsNetworkScanResult, nsINetworkScanResult)

/*============================================================================
 *======================Implementation of Class nsRilResult
 *=====================
 *============================================================================*/
/**
 * Constructor for a nsRilResult
 * For those has no parameter notify.
 */
nsRilResult::nsRilResult(const nsAString& aRilMessageType)
  : mRilMessageType(aRilMessageType)
{
  DEBUG("init nsRilResult for indication.");
}
nsRilResult::nsRilResult(const nsAString& aRilMessageType,
                         int32_t aRilMessageToken,
                         int32_t aErrorMsg)
  : mRilMessageType(aRilMessageType)
  , mRilMessageToken(aRilMessageToken)
  , mErrorMsg(aErrorMsg)
{
  DEBUG("init nsRilResult for response.");
}

/**
 *
 */
nsRilResult::~nsRilResult() {}

// Helper function

int32_t
nsRilResult::convertRadioTechnology(RadioTechnology_V1_0 aRat)
{
  switch (aRat) {
    case RadioTechnology_V1_0::UNKNOWN:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_UNKNOWN;
    case RadioTechnology_V1_0::GPRS:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_GPRS;
    case RadioTechnology_V1_0::EDGE:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_EDGE;
    case RadioTechnology_V1_0::UMTS:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_UMTS;
    case RadioTechnology_V1_0::IS95A:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_IS95A;
    case RadioTechnology_V1_0::IS95B:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_IS95B;
    case RadioTechnology_V1_0::ONE_X_RTT:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_1XRTT;
    case RadioTechnology_V1_0::EVDO_0:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_EVDO0;
    case RadioTechnology_V1_0::EVDO_A:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_EVDOA;
    case RadioTechnology_V1_0::HSDPA:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_HSDPA;
    case RadioTechnology_V1_0::HSUPA:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_HSUPA;
    case RadioTechnology_V1_0::HSPA:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_HSPA;
    case RadioTechnology_V1_0::EVDO_B:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_EVDOB;
    case RadioTechnology_V1_0::EHRPD:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_EHRPD;
    case RadioTechnology_V1_0::LTE:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_LTE;
    case RadioTechnology_V1_0::HSPAP:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_HSPAP;
    case RadioTechnology_V1_0::GSM:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_GSM;
    case RadioTechnology_V1_0::TD_SCDMA:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_TD_SCDMA;
    case RadioTechnology_V1_0::IWLAN:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_IWLAN;
    case RadioTechnology_V1_0::LTE_CA:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_LTE_CA;
    default:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_UNKNOWN;
  }
}
#if ANDROID_VERSION >= 33
int32_t
nsRilResult::convertRadioTechnology_V1_4(RadioTechnology_V1_4 aRat)
{
  switch (aRat) {
    case RadioTechnology_V1_4::UNKNOWN:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_UNKNOWN;
    case RadioTechnology_V1_4::GPRS:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_GPRS;
    case RadioTechnology_V1_4::EDGE:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_EDGE;
    case RadioTechnology_V1_4::UMTS:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_UMTS;
    case RadioTechnology_V1_4::IS95A:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_IS95A;
    case RadioTechnology_V1_4::IS95B:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_IS95B;
    case RadioTechnology_V1_4::ONE_X_RTT:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_1XRTT;
    case RadioTechnology_V1_4::EVDO_0:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_EVDO0;
    case RadioTechnology_V1_4::EVDO_A:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_EVDOA;
    case RadioTechnology_V1_4::HSDPA:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_HSDPA;
    case RadioTechnology_V1_4::HSUPA:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_HSUPA;
    case RadioTechnology_V1_4::HSPA:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_HSPA;
    case RadioTechnology_V1_4::EVDO_B:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_EVDOB;
    case RadioTechnology_V1_4::EHRPD:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_EHRPD;
    case RadioTechnology_V1_4::LTE:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_LTE;
    case RadioTechnology_V1_4::HSPAP:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_HSPAP;
    case RadioTechnology_V1_4::GSM:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_GSM;
    case RadioTechnology_V1_4::TD_SCDMA:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_TD_SCDMA;
    case RadioTechnology_V1_4::IWLAN:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_IWLAN;
    case RadioTechnology_V1_4::LTE_CA:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_LTE_CA;
    case RadioTechnology_V1_4::NR:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_NR;
    default:
      return nsIRadioTechnologyState::RADIO_CREG_TECH_UNKNOWN;
  }
}

nsString
nsRilResult::convertPdpProtocolType_V1_4(int32_t pdpType)
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
#endif
int32_t
nsRilResult::convertDataCallFailCause(DataCallFailCause_V1_0 aCause)
{
  switch (aCause) {
    case DataCallFailCause_V1_0::NONE:
      return nsIDataCallFailCause::DATACALL_FAIL_NONE;
    case DataCallFailCause_V1_0::OPERATOR_BARRED:
      return nsIDataCallFailCause::DATACALL_FAIL_OPERATOR_BARRED;
    case DataCallFailCause_V1_0::NAS_SIGNALLING:
      return nsIDataCallFailCause::DATACALL_FAIL_NAS_SIGNALLING;
    case DataCallFailCause_V1_0::INSUFFICIENT_RESOURCES:
      return nsIDataCallFailCause::DATACALL_FAIL_INSUFFICIENT_RESOURCES;
    case DataCallFailCause_V1_0::MISSING_UKNOWN_APN:
      return nsIDataCallFailCause::DATACALL_FAIL_MISSING_UKNOWN_APN;
    case DataCallFailCause_V1_0::UNKNOWN_PDP_ADDRESS_TYPE:
      return nsIDataCallFailCause::DATACALL_FAIL_UNKNOWN_PDP_ADDRESS_TYPE;
    case DataCallFailCause_V1_0::USER_AUTHENTICATION:
      return nsIDataCallFailCause::DATACALL_FAIL_USER_AUTHENTICATION;
    case DataCallFailCause_V1_0::ACTIVATION_REJECT_GGSN:
      return nsIDataCallFailCause::DATACALL_FAIL_ACTIVATION_REJECT_GGSN;
    case DataCallFailCause_V1_0::ACTIVATION_REJECT_UNSPECIFIED:
      return nsIDataCallFailCause::DATACALL_FAIL_ACTIVATION_REJECT_UNSPECIFIED;
    case DataCallFailCause_V1_0::SERVICE_OPTION_NOT_SUPPORTED:
      return nsIDataCallFailCause::DATACALL_FAIL_SERVICE_OPTION_NOT_SUPPORTED;
    case DataCallFailCause_V1_0::SERVICE_OPTION_NOT_SUBSCRIBED:
      return nsIDataCallFailCause::DATACALL_FAIL_SERVICE_OPTION_NOT_SUBSCRIBED;
    case DataCallFailCause_V1_0::SERVICE_OPTION_OUT_OF_ORDER:
      return nsIDataCallFailCause::DATACALL_FAIL_SERVICE_OPTION_OUT_OF_ORDER;
    case DataCallFailCause_V1_0::NSAPI_IN_USE:
      return nsIDataCallFailCause::DATACALL_FAIL_NSAPI_IN_USE;
    case DataCallFailCause_V1_0::REGULAR_DEACTIVATION:
      return nsIDataCallFailCause::DATACALL_FAIL_REGULAR_DEACTIVATION;
    case DataCallFailCause_V1_0::QOS_NOT_ACCEPTED:
      return nsIDataCallFailCause::DATACALL_FAIL_QOS_NOT_ACCEPTED;
    case DataCallFailCause_V1_0::NETWORK_FAILURE:
      return nsIDataCallFailCause::DATACALL_FAIL_NETWORK_FAILURE;
    case DataCallFailCause_V1_0::UMTS_REACTIVATION_REQ:
      return nsIDataCallFailCause::DATACALL_FAIL_UMTS_REACTIVATION_REQ;
    case DataCallFailCause_V1_0::FEATURE_NOT_SUPP:
      return nsIDataCallFailCause::DATACALL_FAIL_FEATURE_NOT_SUPP;
    case DataCallFailCause_V1_0::TFT_SEMANTIC_ERROR:
      return nsIDataCallFailCause::DATACALL_FAIL_TFT_SEMANTIC_ERROR;
    case DataCallFailCause_V1_0::TFT_SYTAX_ERROR:
      return nsIDataCallFailCause::DATACALL_FAIL_TFT_SYTAX_ERROR;
    case DataCallFailCause_V1_0::UNKNOWN_PDP_CONTEXT:
      return nsIDataCallFailCause::DATACALL_FAIL_UNKNOWN_PDP_CONTEXT;
    case DataCallFailCause_V1_0::FILTER_SEMANTIC_ERROR:
      return nsIDataCallFailCause::DATACALL_FAIL_FILTER_SEMANTIC_ERROR;
    case DataCallFailCause_V1_0::FILTER_SYTAX_ERROR:
      return nsIDataCallFailCause::DATACALL_FAIL_FILTER_SYTAX_ERROR;
    case DataCallFailCause_V1_0::PDP_WITHOUT_ACTIVE_TFT:
      return nsIDataCallFailCause::DATACALL_FAIL_PDP_WITHOUT_ACTIVE_TFT;
    case DataCallFailCause_V1_0::ONLY_IPV4_ALLOWED:
      return nsIDataCallFailCause::DATACALL_FAIL_ONLY_IPV4_ALLOWED;
    case DataCallFailCause_V1_0::ONLY_IPV6_ALLOWED:
      return nsIDataCallFailCause::DATACALL_FAIL_ONLY_IPV6_ALLOWED;
    case DataCallFailCause_V1_0::ONLY_SINGLE_BEARER_ALLOWED:
      return nsIDataCallFailCause::DATACALL_FAIL_ONLY_SINGLE_BEARER_ALLOWED;
    case DataCallFailCause_V1_0::ESM_INFO_NOT_RECEIVED:
      return nsIDataCallFailCause::DATACALL_FAIL_ESM_INFO_NOT_RECEIVED;
    case DataCallFailCause_V1_0::PDN_CONN_DOES_NOT_EXIST:
      return nsIDataCallFailCause::DATACALL_FAIL_PDN_CONN_DOES_NOT_EXIST;
    case DataCallFailCause_V1_0::MULTI_CONN_TO_SAME_PDN_NOT_ALLOWED:
      return nsIDataCallFailCause::
        DATACALL_FAIL_MULTI_CONN_TO_SAME_PDN_NOT_ALLOWED;
    case DataCallFailCause_V1_0::MAX_ACTIVE_PDP_CONTEXT_REACHED:
      return nsIDataCallFailCause::DATACALL_FAIL_MAX_ACTIVE_PDP_CONTEXT_REACHED;
    case DataCallFailCause_V1_0::UNSUPPORTED_APN_IN_CURRENT_PLMN:
      return nsIDataCallFailCause::
        DATACALL_FAIL_UNSUPPORTED_APN_IN_CURRENT_PLMN;
    case DataCallFailCause_V1_0::INVALID_TRANSACTION_ID:
      return nsIDataCallFailCause::DATACALL_FAIL_INVALID_TRANSACTION_ID;
    case DataCallFailCause_V1_0::MESSAGE_INCORRECT_SEMANTIC:
      return nsIDataCallFailCause::DATACALL_FAIL_MESSAGE_INCORRECT_SEMANTIC;
    case DataCallFailCause_V1_0::INVALID_MANDATORY_INFO:
      return nsIDataCallFailCause::DATACALL_FAIL_INVALID_MANDATORY_INFO;
    case DataCallFailCause_V1_0::MESSAGE_TYPE_UNSUPPORTED:
      return nsIDataCallFailCause::DATACALL_FAIL_MESSAGE_TYPE_UNSUPPORTED;
    case DataCallFailCause_V1_0::MSG_TYPE_NONCOMPATIBLE_STATE:
      return nsIDataCallFailCause::DATACALL_FAIL_MSG_TYPE_NONCOMPATIBLE_STATE;
    case DataCallFailCause_V1_0::UNKNOWN_INFO_ELEMENT:
      return nsIDataCallFailCause::DATACALL_FAIL_UNKNOWN_INFO_ELEMENT;
    case DataCallFailCause_V1_0::CONDITIONAL_IE_ERROR:
      return nsIDataCallFailCause::DATACALL_FAIL_CONDITIONAL_IE_ERROR;
    case DataCallFailCause_V1_0::MSG_AND_PROTOCOL_STATE_UNCOMPATIBLE:
      return nsIDataCallFailCause::
        DATACALL_FAIL_MSG_AND_PROTOCOL_STATE_UNCOMPATIBLE;
    case DataCallFailCause_V1_0::PROTOCOL_ERRORS:
      return nsIDataCallFailCause::DATACALL_FAIL_PROTOCOL_ERRORS;
    case DataCallFailCause_V1_0::APN_TYPE_CONFLICT:
      return nsIDataCallFailCause::DATACALL_FAIL_APN_TYPE_CONFLICT;
    case DataCallFailCause_V1_0::INVALID_PCSCF_ADDR:
      return nsIDataCallFailCause::DATACALL_FAIL_INVALID_PCSCF_ADDR;
    case DataCallFailCause_V1_0::INTERNAL_CALL_PREEMPT_BY_HIGH_PRIO_APN:
      return nsIDataCallFailCause::
        DATACALL_FAIL_INTERNAL_CALL_PREEMPT_BY_HIGH_PRIO_APN;
    case DataCallFailCause_V1_0::EMM_ACCESS_BARRED:
      return nsIDataCallFailCause::DATACALL_FAIL_EMM_ACCESS_BARRED;
    case DataCallFailCause_V1_0::EMERGENCY_IFACE_ONLY:
      return nsIDataCallFailCause::DATACALL_FAIL_EMERGENCY_IFACE_ONLY;
    case DataCallFailCause_V1_0::IFACE_MISMATCH:
      return nsIDataCallFailCause::DATACALL_FAIL_IFACE_MISMATCH;
    case DataCallFailCause_V1_0::COMPANION_IFACE_IN_USE:
      return nsIDataCallFailCause::DATACALL_FAIL_COMPANION_IFACE_IN_USE;
    case DataCallFailCause_V1_0::IP_ADDRESS_MISMATCH:
      return nsIDataCallFailCause::DATACALL_FAIL_IP_ADDRESS_MISMATCH;
    case DataCallFailCause_V1_0::IFACE_AND_POL_FAMILY_MISMATCH:
      return nsIDataCallFailCause::DATACALL_FAIL_IFACE_AND_POL_FAMILY_MISMATCH;
    case DataCallFailCause_V1_0::EMM_ACCESS_BARRED_INFINITE_RETRY:
      return nsIDataCallFailCause::
        DATACALL_FAIL_EMM_ACCESS_BARRED_INFINITE_RETRY;
    case DataCallFailCause_V1_0::AUTH_FAILURE_ON_EMERGENCY_CALL:
      return nsIDataCallFailCause::DATACALL_FAIL_AUTH_FAILURE_ON_EMERGENCY_CALL;
    case DataCallFailCause_V1_0::VOICE_REGISTRATION_FAIL:
      return nsIDataCallFailCause::DATACALL_FAIL_VOICE_REGISTRATION_FAIL;
    case DataCallFailCause_V1_0::DATA_REGISTRATION_FAIL:
      return nsIDataCallFailCause::DATACALL_FAIL_DATA_REGISTRATION_FAIL;
    case DataCallFailCause_V1_0::SIGNAL_LOST:
      return nsIDataCallFailCause::DATACALL_FAIL_SIGNAL_LOST;
    case DataCallFailCause_V1_0::PREF_RADIO_TECH_CHANGED:
      return nsIDataCallFailCause::DATACALL_FAIL_PREF_RADIO_TECH_CHANGED;
    case DataCallFailCause_V1_0::RADIO_POWER_OFF:
      return nsIDataCallFailCause::DATACALL_FAIL_RADIO_POWER_OFF;
    case DataCallFailCause_V1_0::TETHERED_CALL_ACTIVE:
      return nsIDataCallFailCause::DATACALL_FAIL_TETHERED_CALL_ACTIVE;
    case DataCallFailCause_V1_0::ERROR_UNSPECIFIED:
      return nsIDataCallFailCause::DATACALL_FAIL_ERROR_UNSPECIFIED;
    default:
      return nsIDataCallFailCause::DATACALL_FAIL_ERROR_UNSPECIFIED;
  }
}
#if ANDROID_VERSION >= 33
int32_t
nsRilResult::convertDataCallFailCause_1_4(DataCallFailCause_V1_4 aCause)
{
  switch (aCause) {
    case DataCallFailCause_V1_4::NONE:
      return nsIDataCallFailCause::DATACALL_FAIL_NONE;
    case DataCallFailCause_V1_4::OPERATOR_BARRED:
      return nsIDataCallFailCause::DATACALL_FAIL_OPERATOR_BARRED;
    case DataCallFailCause_V1_4::NAS_SIGNALLING:
      return nsIDataCallFailCause::DATACALL_FAIL_NAS_SIGNALLING;
    case DataCallFailCause_V1_4::INSUFFICIENT_RESOURCES:
      return nsIDataCallFailCause::DATACALL_FAIL_INSUFFICIENT_RESOURCES;
    case DataCallFailCause_V1_4::MISSING_UKNOWN_APN:
      return nsIDataCallFailCause::DATACALL_FAIL_MISSING_UKNOWN_APN;
    case DataCallFailCause_V1_4::UNKNOWN_PDP_ADDRESS_TYPE:
      return nsIDataCallFailCause::DATACALL_FAIL_UNKNOWN_PDP_ADDRESS_TYPE;
    case DataCallFailCause_V1_4::USER_AUTHENTICATION:
      return nsIDataCallFailCause::DATACALL_FAIL_USER_AUTHENTICATION;
    case DataCallFailCause_V1_4::ACTIVATION_REJECT_GGSN:
      return nsIDataCallFailCause::DATACALL_FAIL_ACTIVATION_REJECT_GGSN;
    case DataCallFailCause_V1_4::ACTIVATION_REJECT_UNSPECIFIED:
      return nsIDataCallFailCause::DATACALL_FAIL_ACTIVATION_REJECT_UNSPECIFIED;
    case DataCallFailCause_V1_4::SERVICE_OPTION_NOT_SUPPORTED:
      return nsIDataCallFailCause::DATACALL_FAIL_SERVICE_OPTION_NOT_SUPPORTED;
    case DataCallFailCause_V1_4::SERVICE_OPTION_NOT_SUBSCRIBED:
      return nsIDataCallFailCause::DATACALL_FAIL_SERVICE_OPTION_NOT_SUBSCRIBED;
    case DataCallFailCause_V1_4::SERVICE_OPTION_OUT_OF_ORDER:
      return nsIDataCallFailCause::DATACALL_FAIL_SERVICE_OPTION_OUT_OF_ORDER;
    case DataCallFailCause_V1_4::NSAPI_IN_USE:
      return nsIDataCallFailCause::DATACALL_FAIL_NSAPI_IN_USE;
    case DataCallFailCause_V1_4::REGULAR_DEACTIVATION:
      return nsIDataCallFailCause::DATACALL_FAIL_REGULAR_DEACTIVATION;
    case DataCallFailCause_V1_4::QOS_NOT_ACCEPTED:
      return nsIDataCallFailCause::DATACALL_FAIL_QOS_NOT_ACCEPTED;
    case DataCallFailCause_V1_4::NETWORK_FAILURE:
      return nsIDataCallFailCause::DATACALL_FAIL_NETWORK_FAILURE;
    case DataCallFailCause_V1_4::UMTS_REACTIVATION_REQ:
      return nsIDataCallFailCause::DATACALL_FAIL_UMTS_REACTIVATION_REQ;
    case DataCallFailCause_V1_4::FEATURE_NOT_SUPP:
      return nsIDataCallFailCause::DATACALL_FAIL_FEATURE_NOT_SUPP;
    case DataCallFailCause_V1_4::TFT_SEMANTIC_ERROR:
      return nsIDataCallFailCause::DATACALL_FAIL_TFT_SEMANTIC_ERROR;
    case DataCallFailCause_V1_4::TFT_SYTAX_ERROR:
      return nsIDataCallFailCause::DATACALL_FAIL_TFT_SYTAX_ERROR;
    case DataCallFailCause_V1_4::UNKNOWN_PDP_CONTEXT:
      return nsIDataCallFailCause::DATACALL_FAIL_UNKNOWN_PDP_CONTEXT;
    case DataCallFailCause_V1_4::FILTER_SEMANTIC_ERROR:
      return nsIDataCallFailCause::DATACALL_FAIL_FILTER_SEMANTIC_ERROR;
    case DataCallFailCause_V1_4::FILTER_SYTAX_ERROR:
      return nsIDataCallFailCause::DATACALL_FAIL_FILTER_SYTAX_ERROR;
    case DataCallFailCause_V1_4::PDP_WITHOUT_ACTIVE_TFT:
      return nsIDataCallFailCause::DATACALL_FAIL_PDP_WITHOUT_ACTIVE_TFT;
    case DataCallFailCause_V1_4::ONLY_IPV4_ALLOWED:
      return nsIDataCallFailCause::DATACALL_FAIL_ONLY_IPV4_ALLOWED;
    case DataCallFailCause_V1_4::ONLY_IPV6_ALLOWED:
      return nsIDataCallFailCause::DATACALL_FAIL_ONLY_IPV6_ALLOWED;
    case DataCallFailCause_V1_4::ONLY_SINGLE_BEARER_ALLOWED:
      return nsIDataCallFailCause::DATACALL_FAIL_ONLY_SINGLE_BEARER_ALLOWED;
    case DataCallFailCause_V1_4::ESM_INFO_NOT_RECEIVED:
      return nsIDataCallFailCause::DATACALL_FAIL_ESM_INFO_NOT_RECEIVED;
    case DataCallFailCause_V1_4::PDN_CONN_DOES_NOT_EXIST:
      return nsIDataCallFailCause::DATACALL_FAIL_PDN_CONN_DOES_NOT_EXIST;
    case DataCallFailCause_V1_4::MULTI_CONN_TO_SAME_PDN_NOT_ALLOWED:
      return nsIDataCallFailCause::
        DATACALL_FAIL_MULTI_CONN_TO_SAME_PDN_NOT_ALLOWED;
    case DataCallFailCause_V1_4::MAX_ACTIVE_PDP_CONTEXT_REACHED:
      return nsIDataCallFailCause::DATACALL_FAIL_MAX_ACTIVE_PDP_CONTEXT_REACHED;
    case DataCallFailCause_V1_4::UNSUPPORTED_APN_IN_CURRENT_PLMN:
      return nsIDataCallFailCause::
        DATACALL_FAIL_UNSUPPORTED_APN_IN_CURRENT_PLMN;
    case DataCallFailCause_V1_4::INVALID_TRANSACTION_ID:
      return nsIDataCallFailCause::DATACALL_FAIL_INVALID_TRANSACTION_ID;
    case DataCallFailCause_V1_4::MESSAGE_INCORRECT_SEMANTIC:
      return nsIDataCallFailCause::DATACALL_FAIL_MESSAGE_INCORRECT_SEMANTIC;
    case DataCallFailCause_V1_4::INVALID_MANDATORY_INFO:
      return nsIDataCallFailCause::DATACALL_FAIL_INVALID_MANDATORY_INFO;
    case DataCallFailCause_V1_4::MESSAGE_TYPE_UNSUPPORTED:
      return nsIDataCallFailCause::DATACALL_FAIL_MESSAGE_TYPE_UNSUPPORTED;
    case DataCallFailCause_V1_4::MSG_TYPE_NONCOMPATIBLE_STATE:
      return nsIDataCallFailCause::DATACALL_FAIL_MSG_TYPE_NONCOMPATIBLE_STATE;
    case DataCallFailCause_V1_4::UNKNOWN_INFO_ELEMENT:
      return nsIDataCallFailCause::DATACALL_FAIL_UNKNOWN_INFO_ELEMENT;
    case DataCallFailCause_V1_4::CONDITIONAL_IE_ERROR:
      return nsIDataCallFailCause::DATACALL_FAIL_CONDITIONAL_IE_ERROR;
    case DataCallFailCause_V1_4::MSG_AND_PROTOCOL_STATE_UNCOMPATIBLE:
      return nsIDataCallFailCause::
        DATACALL_FAIL_MSG_AND_PROTOCOL_STATE_UNCOMPATIBLE;
    case DataCallFailCause_V1_4::PROTOCOL_ERRORS:
      return nsIDataCallFailCause::DATACALL_FAIL_PROTOCOL_ERRORS;
    case DataCallFailCause_V1_4::APN_TYPE_CONFLICT:
      return nsIDataCallFailCause::DATACALL_FAIL_APN_TYPE_CONFLICT;
    case DataCallFailCause_V1_4::INVALID_PCSCF_ADDR:
      return nsIDataCallFailCause::DATACALL_FAIL_INVALID_PCSCF_ADDR;
    case DataCallFailCause_V1_4::INTERNAL_CALL_PREEMPT_BY_HIGH_PRIO_APN:
      return nsIDataCallFailCause::
        DATACALL_FAIL_INTERNAL_CALL_PREEMPT_BY_HIGH_PRIO_APN;
    case DataCallFailCause_V1_4::EMM_ACCESS_BARRED:
      return nsIDataCallFailCause::DATACALL_FAIL_EMM_ACCESS_BARRED;
    case DataCallFailCause_V1_4::EMERGENCY_IFACE_ONLY:
      return nsIDataCallFailCause::DATACALL_FAIL_EMERGENCY_IFACE_ONLY;
    case DataCallFailCause_V1_4::IFACE_MISMATCH:
      return nsIDataCallFailCause::DATACALL_FAIL_IFACE_MISMATCH;
    case DataCallFailCause_V1_4::COMPANION_IFACE_IN_USE:
      return nsIDataCallFailCause::DATACALL_FAIL_COMPANION_IFACE_IN_USE;
    case DataCallFailCause_V1_4::IP_ADDRESS_MISMATCH:
      return nsIDataCallFailCause::DATACALL_FAIL_IP_ADDRESS_MISMATCH;
    case DataCallFailCause_V1_4::IFACE_AND_POL_FAMILY_MISMATCH:
      return nsIDataCallFailCause::DATACALL_FAIL_IFACE_AND_POL_FAMILY_MISMATCH;
    case DataCallFailCause_V1_4::EMM_ACCESS_BARRED_INFINITE_RETRY:
      return nsIDataCallFailCause::
        DATACALL_FAIL_EMM_ACCESS_BARRED_INFINITE_RETRY;
    case DataCallFailCause_V1_4::AUTH_FAILURE_ON_EMERGENCY_CALL:
      return nsIDataCallFailCause::DATACALL_FAIL_AUTH_FAILURE_ON_EMERGENCY_CALL;
    case DataCallFailCause_V1_4::VOICE_REGISTRATION_FAIL:
      return nsIDataCallFailCause::DATACALL_FAIL_VOICE_REGISTRATION_FAIL;
    case DataCallFailCause_V1_4::DATA_REGISTRATION_FAIL:
      return nsIDataCallFailCause::DATACALL_FAIL_DATA_REGISTRATION_FAIL;
    case DataCallFailCause_V1_4::SIGNAL_LOST:
      return nsIDataCallFailCause::DATACALL_FAIL_SIGNAL_LOST;
    case DataCallFailCause_V1_4::PREF_RADIO_TECH_CHANGED:
      return nsIDataCallFailCause::DATACALL_FAIL_PREF_RADIO_TECH_CHANGED;
    case DataCallFailCause_V1_4::RADIO_POWER_OFF:
      return nsIDataCallFailCause::DATACALL_FAIL_RADIO_POWER_OFF;
    case DataCallFailCause_V1_4::TETHERED_CALL_ACTIVE:
      return nsIDataCallFailCause::DATACALL_FAIL_TETHERED_CALL_ACTIVE;
    case DataCallFailCause_V1_4::ERROR_UNSPECIFIED:
      return nsIDataCallFailCause::DATACALL_FAIL_ERROR_UNSPECIFIED;
    case DataCallFailCause_V1_4::LLC_SNDCP:
      return nsIDataCallFailCause::DATACALL_FAIL_LLC_SNDCP;
    default:
      return nsIDataCallFailCause::DATACALL_FAIL_ERROR_UNSPECIFIED;
  }
}

RefPtr<nsCellIdentityOperatorNames>
nsRilResult::convertCellIdentityOperatorNames(
  const CellIdentityOperatorNames_V1_2& aOperatorNames)
{
  RefPtr<nsCellIdentityOperatorNames> names = new nsCellIdentityOperatorNames(
    NS_ConvertUTF8toUTF16(aOperatorNames.alphaLong.c_str()),
    NS_ConvertUTF8toUTF16(aOperatorNames.alphaShort.c_str()));
  return names;
}
#endif
RefPtr<nsCellIdentity>
nsRilResult::convertCellIdentity(const CellIdentity_V1_0* aCellIdentity)
{
  int32_t cellInfoType = convertCellInfoType(aCellIdentity->cellInfoType);
  if (aCellIdentity->cellInfoType == CellInfoType::GSM) {
    uint32_t numCellIdentityGsm = aCellIdentity->cellIdentityGsm.size();
    if (numCellIdentityGsm > 0) {
      RefPtr<nsCellIdentityGsm> cellIdentityGsm =
        convertCellIdentityGsm(&aCellIdentity->cellIdentityGsm[0]);
      RefPtr<nsCellIdentity> cellIdentity =
        new nsCellIdentity(cellInfoType, cellIdentityGsm);
      return cellIdentity;
    }
  } else if (aCellIdentity->cellInfoType == CellInfoType::LTE) {
    uint32_t numCellIdentityLte = aCellIdentity->cellIdentityLte.size();
    if (numCellIdentityLte > 0) {
      RefPtr<nsCellIdentityLte> cellIdentityLte =
        convertCellIdentityLte(&aCellIdentity->cellIdentityLte[0]);
      RefPtr<nsCellIdentity> cellIdentity =
        new nsCellIdentity(cellInfoType, cellIdentityLte);
      return cellIdentity;
    }
  } else if (aCellIdentity->cellInfoType == CellInfoType::WCDMA) {
    uint32_t numCellIdentityWcdma = aCellIdentity->cellIdentityWcdma.size();
    if (numCellIdentityWcdma > 0) {
      RefPtr<nsCellIdentityWcdma> cellIdentityWcdma =
        convertCellIdentityWcdma(&aCellIdentity->cellIdentityWcdma[0]);
      RefPtr<nsCellIdentity> cellIdentity =
        new nsCellIdentity(cellInfoType, cellIdentityWcdma);
      return cellIdentity;
    }
  } else if (aCellIdentity->cellInfoType == CellInfoType::TD_SCDMA) {
    uint32_t numCellIdentityTdScdma = aCellIdentity->cellIdentityTdscdma.size();
    if (numCellIdentityTdScdma > 0) {
      RefPtr<nsCellIdentityTdScdma> cellIdentityTdScdma =
        convertCellIdentityTdScdma(&aCellIdentity->cellIdentityTdscdma[0]);
      RefPtr<nsCellIdentity> cellIdentity =
        new nsCellIdentity(cellInfoType, cellIdentityTdScdma);
      return cellIdentity;
    }
  } else if (aCellIdentity->cellInfoType == CellInfoType::CDMA) {
    uint32_t numCellIdentityCdma = aCellIdentity->cellIdentityCdma.size();
    if (numCellIdentityCdma > 0) {
      RefPtr<nsCellIdentityCdma> cellIdentityCdma =
        convertCellIdentityCdma(&aCellIdentity->cellIdentityCdma[0]);
      RefPtr<nsCellIdentity> cellIdentity =
        new nsCellIdentity(cellInfoType, cellIdentityCdma);
      return cellIdentity;
    }
  }
  return nullptr;
}
#if ANDROID_VERSION >= 33
RefPtr<nsCellIdentity>
nsRilResult::convertCellIdentity_V1_2(
  const IRadioCellIdentity_V1_2* aCellIdentity)
{
  int32_t cellInfoType = convertCellInfoType(aCellIdentity->cellInfoType);
  if (aCellIdentity->cellInfoType == CellInfoType::GSM) {
    uint32_t numCellIdentityGsm = aCellIdentity->cellIdentityGsm.size();

    if (numCellIdentityGsm > 0) {
      RefPtr<nsCellIdentityGsm> cellIdentityGsm =
        convertCellIdentityGsm_V1_2(&aCellIdentity->cellIdentityGsm[0]);
      RefPtr<nsCellIdentity> cellIdentity =
        new nsCellIdentity(cellInfoType, cellIdentityGsm);
      return cellIdentity;
    }
  } else if (aCellIdentity->cellInfoType == CellInfoType::LTE) {
    uint32_t numCellIdentityLte = aCellIdentity->cellIdentityLte.size();
    if (numCellIdentityLte > 0) {
      RefPtr<nsCellIdentityLte> cellIdentityLte =
        convertCellIdentityLte_V1_2(&aCellIdentity->cellIdentityLte[0]);
      RefPtr<nsCellIdentity> cellIdentity =
        new nsCellIdentity(cellInfoType, cellIdentityLte);
      return cellIdentity;
    }
  } else if (aCellIdentity->cellInfoType == CellInfoType::WCDMA) {
    uint32_t numCellIdentityWcdma = aCellIdentity->cellIdentityWcdma.size();
    if (numCellIdentityWcdma > 0) {
      RefPtr<nsCellIdentityWcdma> cellIdentityWcdma =
        convertCellIdentityWcdma_V1_2(&aCellIdentity->cellIdentityWcdma[0]);
      RefPtr<nsCellIdentity> cellIdentity =
        new nsCellIdentity(cellInfoType, cellIdentityWcdma);
      return cellIdentity;
    }
  } else if (aCellIdentity->cellInfoType == CellInfoType::TD_SCDMA) {
    uint32_t numCellIdentityTdScdma = aCellIdentity->cellIdentityTdscdma.size();
    if (numCellIdentityTdScdma > 0) {
      RefPtr<nsCellIdentityTdScdma> cellIdentityTdScdma =
        convertCellIdentityTdScdma_V1_2(&aCellIdentity->cellIdentityTdscdma[0]);
      RefPtr<nsCellIdentity> cellIdentity =
        new nsCellIdentity(cellInfoType, cellIdentityTdScdma);
      return cellIdentity;
    }
  } else if (aCellIdentity->cellInfoType == CellInfoType::CDMA) {
    uint32_t numCellIdentityCdma = aCellIdentity->cellIdentityCdma.size();
    if (numCellIdentityCdma > 0) {
      RefPtr<nsCellIdentityCdma> cellIdentityCdma =
        convertCellIdentityCdma_V1_2(&aCellIdentity->cellIdentityCdma[0]);
      RefPtr<nsCellIdentity> cellIdentity =
        new nsCellIdentity(cellInfoType, cellIdentityCdma);
      return cellIdentity;
    }
  }
  return nullptr;
}
#endif
RefPtr<nsCellIdentityGsm>
nsRilResult::convertCellIdentityGsm(
  const CellIdentityGsm_V1_0* aCellIdentityGsm)
{
  nsString mcc = NS_ConvertUTF8toUTF16(aCellIdentityGsm->mcc.c_str());
  nsString mnc = NS_ConvertUTF8toUTF16(aCellIdentityGsm->mnc.c_str());
  int32_t lac = aCellIdentityGsm->lac;
  int32_t cid = aCellIdentityGsm->cid;
  int32_t arfcn = aCellIdentityGsm->arfcn;
  int32_t bsic = aCellIdentityGsm->bsic;
  nsTArray<nsString> additionalPlmns;
  nsString addPlmn;
  addPlmn.Assign(NS_ConvertUTF8toUTF16(""));
  additionalPlmns.AppendElement(addPlmn);
  RefPtr<nsCellIdentityGsm> gsm = new nsCellIdentityGsm(
    mcc, mnc, lac, cid, arfcn, bsic, nullptr, additionalPlmns);
  return gsm;
}
#if ANDROID_VERSION >= 33
RefPtr<nsCellIdentityGsm>
nsRilResult::convertCellIdentityGsm_V1_2(
  const IRadioCellIdentityGsm_V1_2* aCellIdentityGsm)
{
  nsString mcc = NS_ConvertUTF8toUTF16(aCellIdentityGsm->base.mcc.c_str());
  nsString mnc = NS_ConvertUTF8toUTF16(aCellIdentityGsm->base.mnc.c_str());
  int32_t lac = aCellIdentityGsm->base.lac;
  int32_t cid = aCellIdentityGsm->base.cid;
  int32_t arfcn = aCellIdentityGsm->base.arfcn;
  int32_t bsic = aCellIdentityGsm->base.bsic;
  RefPtr<nsCellIdentityOperatorNames> operatorNames =
    convertCellIdentityOperatorNames(aCellIdentityGsm->operatorNames);
  nsTArray<nsString> additionalPlmns;
  nsString addPlmn;
  addPlmn.Assign(NS_ConvertUTF8toUTF16(""));
  additionalPlmns.AppendElement(addPlmn);
  RefPtr<nsCellIdentityGsm> gsm = new nsCellIdentityGsm(
    mcc, mnc, lac, cid, arfcn, bsic, operatorNames, additionalPlmns);
  return gsm;
}
#endif
RefPtr<nsCellIdentityLte>
nsRilResult::convertCellIdentityLte(
  const CellIdentityLte_V1_0* aCellIdentityLte)
{
  nsString mcc = NS_ConvertUTF8toUTF16(aCellIdentityLte->mcc.c_str());
  nsString mnc = NS_ConvertUTF8toUTF16(aCellIdentityLte->mnc.c_str());
  int32_t ci = aCellIdentityLte->ci;
  int32_t pci = aCellIdentityLte->pci;
  int32_t tac = aCellIdentityLte->tac;
  int32_t earfcn = aCellIdentityLte->earfcn;

  nsTArray<nsString> additionalPlmns;
  nsString addPlmn;
  addPlmn.Assign(NS_ConvertUTF8toUTF16(""));
  additionalPlmns.AppendElement(addPlmn);

  nsTArray<int32_t> bands;
  int32_t band = -1;
  bands.AppendElement(band);

  RefPtr<nsCellIdentityLte> lte = new nsCellIdentityLte(mcc,
                                                        mnc,
                                                        ci,
                                                        pci,
                                                        tac,
                                                        earfcn,
                                                        nullptr,
                                                        -1,
                                                        additionalPlmns,
                                                        nullptr,
                                                        bands);
  return lte;
}
#if ANDROID_VERSION >= 33
RefPtr<nsCellIdentityLte>
nsRilResult::convertCellIdentityLte_V1_2(
  const IRadioCellIdentityLte_V1_2* aCellIdentityLte)
{
  nsString mcc;
  nsString mnc;
  int32_t ci;
  int32_t pci;
  int32_t tac;
  int32_t earfcn;
  int32_t bandwidth = 0;
  RefPtr<nsCellIdentityOperatorNames> operatorNames;

  mcc = NS_ConvertUTF8toUTF16(aCellIdentityLte->base.mcc.c_str());
  mnc = NS_ConvertUTF8toUTF16(aCellIdentityLte->base.mnc.c_str());
  ci = aCellIdentityLte->base.ci;
  pci = aCellIdentityLte->base.pci;
  tac = aCellIdentityLte->base.tac;
  earfcn = aCellIdentityLte->base.earfcn;
  operatorNames =
    convertCellIdentityOperatorNames(aCellIdentityLte->operatorNames);
  bandwidth = aCellIdentityLte->bandwidth;

  nsTArray<nsString> additionalPlmns;
  nsString addPlmn;
  addPlmn.Assign(NS_ConvertUTF8toUTF16(""));
  additionalPlmns.AppendElement(addPlmn);

  nsTArray<int32_t> bands;
  int32_t band = -1;
  bands.AppendElement(band);

  RefPtr<nsCellIdentityLte> lte = new nsCellIdentityLte(mcc,
                                                        mnc,
                                                        ci,
                                                        pci,
                                                        tac,
                                                        earfcn,
                                                        operatorNames,
                                                        bandwidth,
                                                        additionalPlmns,
                                                        nullptr,
                                                        bands);
  return lte;
}
#endif
RefPtr<nsCellIdentityWcdma>
nsRilResult::convertCellIdentityWcdma(
  const CellIdentityWcdma_V1_0* aCellIdentityWcdma)
{
  nsString mcc = NS_ConvertUTF8toUTF16(aCellIdentityWcdma->mcc.c_str());
  nsString mnc = NS_ConvertUTF8toUTF16(aCellIdentityWcdma->mnc.c_str());
  int32_t lac = aCellIdentityWcdma->lac;
  int32_t cid = aCellIdentityWcdma->cid;
  int32_t psc = aCellIdentityWcdma->psc;
  int32_t uarfcn = aCellIdentityWcdma->uarfcn;
  nsTArray<nsString> additionalPlmns;
  nsString addPlmn;
  addPlmn.Assign(NS_ConvertUTF8toUTF16(""));
  additionalPlmns.AppendElement(addPlmn);

  RefPtr<nsCellIdentityWcdma> wcdma = new nsCellIdentityWcdma(
    mcc, mnc, lac, cid, psc, uarfcn, nullptr, additionalPlmns, nullptr);
  return wcdma;
}
#if ANDROID_VERSION >= 33
RefPtr<nsCellIdentityWcdma>
nsRilResult::convertCellIdentityWcdma_V1_2(
  const IRadioCellIdentityWcdma_V1_2* aCellIdentityWcdma)
{
  nsString mcc = NS_ConvertUTF8toUTF16(aCellIdentityWcdma->base.mcc.c_str());
  nsString mnc = NS_ConvertUTF8toUTF16(aCellIdentityWcdma->base.mnc.c_str());
  int32_t lac = aCellIdentityWcdma->base.lac;
  int32_t cid = aCellIdentityWcdma->base.cid;
  int32_t psc = aCellIdentityWcdma->base.psc;
  int32_t uarfcn = aCellIdentityWcdma->base.uarfcn;
  RefPtr<nsCellIdentityOperatorNames> operatorNames =
    convertCellIdentityOperatorNames(aCellIdentityWcdma->operatorNames);
  nsTArray<nsString> additionalPlmns;
  nsString addPlmn;
  addPlmn.Assign(NS_ConvertUTF8toUTF16(""));
  additionalPlmns.AppendElement(addPlmn);
  RefPtr<nsCellIdentityWcdma> wcdma = new nsCellIdentityWcdma(
    mcc, mnc, lac, cid, psc, uarfcn, operatorNames, additionalPlmns, nullptr);
  return wcdma;
}
#endif
RefPtr<nsCellIdentityTdScdma>
nsRilResult::convertCellIdentityTdScdma(
  const CellIdentityTdscdma_V1_0* aCellIdentityTdScdma)
{
  nsString mcc = NS_ConvertUTF8toUTF16(aCellIdentityTdScdma->mcc.c_str());
  nsString mnc = NS_ConvertUTF8toUTF16(aCellIdentityTdScdma->mnc.c_str());
  int32_t lac = aCellIdentityTdScdma->lac;
  int32_t cid = aCellIdentityTdScdma->cid;
  int32_t cpid = aCellIdentityTdScdma->cpid;
  nsTArray<nsString> additionalPlmns;
  nsString addPlmn;
  addPlmn.Assign(NS_ConvertUTF8toUTF16(""));
  additionalPlmns.AppendElement(addPlmn);

  RefPtr<nsCellIdentityTdScdma> tdscdma = new nsCellIdentityTdScdma(
    mcc, mnc, lac, cid, cpid, nullptr, 0, additionalPlmns, nullptr);
  return tdscdma;
}
#if ANDROID_VERSION >= 33
RefPtr<nsCellIdentityTdScdma>
nsRilResult::convertCellIdentityTdScdma_V1_2(
  const IRadioCellIdentityTdscdma_V1_2* aCellIdentityTdScdma)
{
  nsString mcc = NS_ConvertUTF8toUTF16(aCellIdentityTdScdma->base.mcc.c_str());
  nsString mnc = NS_ConvertUTF8toUTF16(aCellIdentityTdScdma->base.mnc.c_str());
  int32_t lac = aCellIdentityTdScdma->base.lac;
  int32_t cid = aCellIdentityTdScdma->base.cid;
  int32_t cpid = aCellIdentityTdScdma->base.cpid;
  int32_t uarfcn = aCellIdentityTdScdma->uarfcn;
  RefPtr<nsCellIdentityOperatorNames> operatorNames =
    convertCellIdentityOperatorNames(aCellIdentityTdScdma->operatorNames);

  nsTArray<nsString> additionalPlmns;
  nsString addPlmn;
  addPlmn.Assign(NS_ConvertUTF8toUTF16(""));
  additionalPlmns.AppendElement(addPlmn);

  RefPtr<nsCellIdentityTdScdma> tdscdma = new nsCellIdentityTdScdma(
    mcc, mnc, lac, cid, cpid, operatorNames, uarfcn, additionalPlmns, nullptr);
  return tdscdma;
}

RefPtr<nsCellIdentityNr>
nsRilResult::convertCellIdentityNr(
  const IRadioCellIdentityNr_V1_4* aCellIdentityNr)
{
  nsString mcc = NS_ConvertUTF8toUTF16(aCellIdentityNr->mcc.c_str());
  nsString mnc = NS_ConvertUTF8toUTF16(aCellIdentityNr->mnc.c_str());
  RefPtr<nsCellIdentityOperatorNames> operatorNames =
    convertCellIdentityOperatorNames(aCellIdentityNr->operatorNames);

  nsTArray<nsString> additionalPlmns;
  nsString addPlmn;
  addPlmn.Assign(NS_ConvertUTF8toUTF16(""));
  additionalPlmns.AppendElement(addPlmn);

  nsTArray<int32_t> bands;
  int32_t band = -1;
  bands.AppendElement(band);

  RefPtr<nsCellIdentityNr> nr = new nsCellIdentityNr(mcc,
                                                     mnc,
                                                     aCellIdentityNr->nci,
                                                     aCellIdentityNr->pci,
                                                     aCellIdentityNr->tac,
                                                     aCellIdentityNr->nrarfcn,
                                                     operatorNames,
                                                     additionalPlmns,
                                                     bands);
  return nr;
}
#endif
RefPtr<nsCellIdentityCdma>
nsRilResult::convertCellIdentityCdma(
  const CellIdentityCdma_V1_0* aCellIdentityCdma)
{
  int32_t networkId = aCellIdentityCdma->networkId;
  int32_t systemId = aCellIdentityCdma->systemId;
  int32_t baseStationId = aCellIdentityCdma->baseStationId;
  int32_t longitude = aCellIdentityCdma->longitude;
  int32_t latitude = aCellIdentityCdma->latitude;

  RefPtr<nsCellIdentityCdma> cdma = new nsCellIdentityCdma(
    networkId, systemId, baseStationId, longitude, latitude, nullptr);
  return cdma;
}
#if ANDROID_VERSION >= 33
RefPtr<nsCellIdentityCdma>
nsRilResult::convertCellIdentityCdma_V1_2(
  const IRadioCellIdentityCdma_V1_2* aCellIdentityCdma)
{

  int32_t networkId = aCellIdentityCdma->base.networkId;
  int32_t systemId = aCellIdentityCdma->base.systemId;
  int32_t baseStationId = aCellIdentityCdma->base.baseStationId;
  int32_t longitude = aCellIdentityCdma->base.longitude;
  int32_t latitude = aCellIdentityCdma->base.latitude;
  RefPtr<nsCellIdentityOperatorNames> operatorNames =
    convertCellIdentityOperatorNames(aCellIdentityCdma->operatorNames);

  RefPtr<nsCellIdentityCdma> cdma = new nsCellIdentityCdma(
    networkId, systemId, baseStationId, longitude, latitude, operatorNames);
  return cdma;
}
#endif
RefPtr<nsSignalStrength>
nsRilResult::convertSignalStrength(const SignalStrength& aSignalStrength)
{
  RefPtr<nsGsmSignalStrength> gsmSignalStrength =
    convertGsmSignalStrength(aSignalStrength.gw);
  RefPtr<nsCdmaSignalStrength> cdmaSignalStrength =
    convertCdmaSignalStrength(aSignalStrength.cdma);
  RefPtr<nsEvdoSignalStrength> evdoSignalStrength =
    convertEvdoSignalStrength(aSignalStrength.evdo);
  RefPtr<nsLteSignalStrength> lteSignalStrength =
    convertLteSignalStrength(aSignalStrength.lte);
  RefPtr<nsTdScdmaSignalStrength> tdscdmaSignalStrength =
    convertTdScdmaSignalStrength(aSignalStrength.tdScdma);

  RefPtr<nsSignalStrength> signalStrength =
    new nsSignalStrength(gsmSignalStrength,
                         cdmaSignalStrength,
                         evdoSignalStrength,
                         lteSignalStrength,
                         tdscdmaSignalStrength,
                         nullptr,
                         nullptr);

  return signalStrength;
}
#if ANDROID_VERSION >= 33
RefPtr<nsSignalStrength>
nsRilResult::convertSignalStrength_V1_2(
  const IRadioSignalStrength_V1_2& aSignalStrength)
{
  RefPtr<nsGsmSignalStrength> gsmSignalStrength =
    convertGsmSignalStrength(aSignalStrength.gsm);
  RefPtr<nsCdmaSignalStrength> cdmaSignalStrength =
    convertCdmaSignalStrength(aSignalStrength.cdma);
  RefPtr<nsEvdoSignalStrength> evdoSignalStrength =
    convertEvdoSignalStrength(aSignalStrength.evdo);
  RefPtr<nsLteSignalStrength> lteSignalStrength =
    convertLteSignalStrength(aSignalStrength.lte);
  // This is a Bug in Google aidl V1.2 define. It use V1.0 define here.
  RefPtr<nsTdScdmaSignalStrength> tdscdmaSignalStrength =
    convertTdScdmaSignalStrength(aSignalStrength.tdScdma);
  //
  RefPtr<nsWcdmaSignalStrength> wcdmaSignalStrength =
    convertWcdmaSignalStrength_V1_2(aSignalStrength.wcdma);

  RefPtr<nsSignalStrength> signalStrength =
    new nsSignalStrength(gsmSignalStrength,
                         cdmaSignalStrength,
                         evdoSignalStrength,
                         lteSignalStrength,
                         tdscdmaSignalStrength,
                         wcdmaSignalStrength,
                         nullptr);

  return signalStrength;
}

RefPtr<nsSignalStrength>
nsRilResult::convertSignalStrength_V1_4(
  const ISignalStrength_V1_4& aSignalStrength)
{
  RefPtr<nsGsmSignalStrength> gsmSignalStrength =
    convertGsmSignalStrength(aSignalStrength.gsm);
  RefPtr<nsCdmaSignalStrength> cdmaSignalStrength =
    convertCdmaSignalStrength(aSignalStrength.cdma);
  RefPtr<nsEvdoSignalStrength> evdoSignalStrength =
    convertEvdoSignalStrength(aSignalStrength.evdo);
  RefPtr<nsWcdmaSignalStrength> wcdmaSignalStrength =
    convertWcdmaSignalStrength_V1_2(aSignalStrength.wcdma);
  RefPtr<nsTdScdmaSignalStrength> tdscdmaSignalStrength =
    convertTdScdmaSignalStrength_V1_2(aSignalStrength.tdscdma);
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
#endif
RefPtr<nsGsmSignalStrength>
nsRilResult::convertGsmSignalStrength(
  const GsmSignalStrength& aGsmSignalStrength)
{
  RefPtr<nsGsmSignalStrength> gsmSignalStrength =
    new nsGsmSignalStrength(aGsmSignalStrength.signalStrength,
                            aGsmSignalStrength.bitErrorRate,
                            aGsmSignalStrength.timingAdvance);

  return gsmSignalStrength;
}

RefPtr<nsWcdmaSignalStrength>
nsRilResult::convertWcdmaSignalStrength(
  const WcdmaSignalStrength& aWcdmaSignalStrength)
{
  RefPtr<nsWcdmaSignalStrength> wcdmaSignalStrength =
    new nsWcdmaSignalStrength(aWcdmaSignalStrength.signalStrength,
                              aWcdmaSignalStrength.bitErrorRate,
                              -1,
                              -1);

  return wcdmaSignalStrength;
}
#if ANDROID_VERSION >= 33
RefPtr<nsWcdmaSignalStrength>
nsRilResult::convertWcdmaSignalStrength_V1_2(
  const IRadioWcdmaSignalStrength_V1_2& aWcdmaSignalStrength)
{
  RefPtr<nsWcdmaSignalStrength> wcdmaSignalStrength =
    new nsWcdmaSignalStrength(aWcdmaSignalStrength.base.signalStrength,
                              aWcdmaSignalStrength.base.bitErrorRate,
                              aWcdmaSignalStrength.rscp,
                              aWcdmaSignalStrength.ecno);

  return wcdmaSignalStrength;
}
#endif
RefPtr<nsCdmaSignalStrength>
nsRilResult::convertCdmaSignalStrength(
  const CdmaSignalStrength_V1_0& aCdmaSignalStrength)
{
  RefPtr<nsCdmaSignalStrength> cdmaSignalStrength =
    new nsCdmaSignalStrength(aCdmaSignalStrength.dbm, aCdmaSignalStrength.ecio);

  return cdmaSignalStrength;
}
RefPtr<nsEvdoSignalStrength>
nsRilResult::convertEvdoSignalStrength(
  const EvdoSignalStrength& aEvdoSignalStrength)
{
  RefPtr<nsEvdoSignalStrength> evdoSignalStrength =
    new nsEvdoSignalStrength(aEvdoSignalStrength.dbm,
                             aEvdoSignalStrength.ecio,
                             aEvdoSignalStrength.signalNoiseRatio);

  return evdoSignalStrength;
}

RefPtr<nsLteSignalStrength>
nsRilResult::convertLteSignalStrength(
  const LteSignalStrength& aLteSignalStrength)
{
  RefPtr<nsLteSignalStrength> lteSignalStrength =
    new nsLteSignalStrength(aLteSignalStrength.signalStrength,
                            aLteSignalStrength.rsrp,
                            aLteSignalStrength.rsrq,
                            aLteSignalStrength.rssnr,
                            aLteSignalStrength.cqi,
                            aLteSignalStrength.timingAdvance,
                            0);

  return lteSignalStrength;
}
#if ANDROID_VERSION >= 33
RefPtr<nsCellConfigLte>
nsRilResult::convertCellConfigLte(const IRadioCellConfigLte_V1_4& aCellConfig)
{
  RefPtr<nsCellConfigLte> cellConfigLte =
    new nsCellConfigLte(aCellConfig.isEndcAvailable);

  return cellConfigLte;
}
#endif
RefPtr<nsTdScdmaSignalStrength>
nsRilResult::convertTdScdmaSignalStrength(
  const TdScdmaSignalStrength& aTdScdmaSignalStrength)
{
  RefPtr<nsTdScdmaSignalStrength> tdscdmaSignalStrength =
    new nsTdScdmaSignalStrength(0, 0, aTdScdmaSignalStrength.rscp);

  return tdscdmaSignalStrength;
}
#if ANDROID_VERSION >= 33
RefPtr<nsNrSignalStrength>
nsRilResult::convertNrSignalStrength(
  const IRadioNrSignalStrength_V1_4* aNrSignalStrength)
{
  nsTArray<int32_t> aCsiCqiReport;
  int32_t cqi = -1;
  aCsiCqiReport.AppendElement(cqi);
  RefPtr<nsNrSignalStrength> nrSignalStrength =
    new nsNrSignalStrength(aNrSignalStrength->ssRsrp,
                           aNrSignalStrength->ssRsrq,
                           aNrSignalStrength->ssSinr,
                           aNrSignalStrength->csiRsrp,
                           aNrSignalStrength->csiRsrq,
                           aNrSignalStrength->csiSinr,
                           0,
                           aCsiCqiReport);

  return nrSignalStrength;
}

RefPtr<nsTdScdmaSignalStrength>
nsRilResult::convertTdScdmaSignalStrength_V1_2(
  const IRadioTdScdmaSignalStrength_V1_2& aTdScdmaSignalStrength)
{
  RefPtr<nsTdScdmaSignalStrength> tdscdmaSignalStrength =
    new nsTdScdmaSignalStrength(aTdScdmaSignalStrength.signalStrength,
                                aTdScdmaSignalStrength.bitErrorRate,
                                aTdScdmaSignalStrength.rscp);

  return tdscdmaSignalStrength;
}
#endif
RefPtr<nsRilCellInfo>
nsRilResult::convertRilCellInfo(const CellInfo_V1_0* aCellInfo)
{
  int32_t connectionStatus = -1;
  int32_t cellInfoType = convertCellInfoType(aCellInfo->cellInfoType);
  bool registered = aCellInfo->registered;
  int32_t timeStampType = convertTimeStampType(aCellInfo->timeStampType);
  int32_t timeStamp = aCellInfo->timeStamp;

  if (aCellInfo->cellInfoType == CellInfoType::GSM) {
    RefPtr<nsCellInfoGsm> cellInfoGsm = convertCellInfoGsm(&aCellInfo->gsm[0]);
    RefPtr<nsRilCellInfo> rilCellInfoGsm = new nsRilCellInfo(cellInfoType,
                                                             registered,
                                                             timeStampType,
                                                             timeStamp,
                                                             cellInfoGsm,
                                                             connectionStatus);
    return rilCellInfoGsm;
  } else if (aCellInfo->cellInfoType == CellInfoType::LTE) {
    RefPtr<nsCellInfoLte> cellInfoLte = convertCellInfoLte(&aCellInfo->lte[0]);
    RefPtr<nsRilCellInfo> rilCellInfoLte = new nsRilCellInfo(cellInfoType,
                                                             registered,
                                                             timeStampType,
                                                             timeStamp,
                                                             cellInfoLte,
                                                             connectionStatus);
    return rilCellInfoLte;
  } else if (aCellInfo->cellInfoType == CellInfoType::WCDMA) {
    RefPtr<nsCellInfoWcdma> cellInfoWcdma =
      convertCellInfoWcdma(&aCellInfo->wcdma[0]);
    RefPtr<nsRilCellInfo> rilCellInfoWcdma =
      new nsRilCellInfo(cellInfoType,
                        registered,
                        timeStampType,
                        timeStamp,
                        cellInfoWcdma,
                        connectionStatus);
    return rilCellInfoWcdma;
  } else if (aCellInfo->cellInfoType == CellInfoType::TD_SCDMA) {
    RefPtr<nsCellInfoTdScdma> cellInfoTdScdma =
      convertCellInfoTdScdma(&aCellInfo->tdscdma[0]);
    RefPtr<nsRilCellInfo> rilCellInfoTdScdma =
      new nsRilCellInfo(cellInfoType,
                        registered,
                        timeStampType,
                        timeStamp,
                        cellInfoTdScdma,
                        connectionStatus);
    return rilCellInfoTdScdma;
  } else if (aCellInfo->cellInfoType == CellInfoType::CDMA) {
    RefPtr<nsCellInfoCdma> cellInfoCdma =
      convertCellInfoCdma(&aCellInfo->cdma[0]);
    RefPtr<nsRilCellInfo> rilCellInfoCdma = new nsRilCellInfo(cellInfoType,
                                                              registered,
                                                              timeStampType,
                                                              timeStamp,
                                                              cellInfoCdma,
                                                              connectionStatus);
    return rilCellInfoCdma;
  } else {
    return nullptr;
  }
}
#if ANDROID_VERSION >= 33
RefPtr<nsRilCellInfo>
nsRilResult::convertRilCellInfo_V1_2(const IRadioCellInfo_V1_2* aCellInfo)
{
  int32_t cellInfoType = convertCellInfoType(aCellInfo->cellInfoType);
  bool registered = aCellInfo->registered;
  int32_t timeStampType = convertTimeStampType(aCellInfo->timeStampType);
  int32_t timeStamp = aCellInfo->timeStamp;
  int32_t connectionStatus =
    convertConnectionStatus(aCellInfo->connectionStatus);

  if (aCellInfo->cellInfoType == CellInfoType::GSM) {
    RefPtr<nsCellInfoGsm> cellInfoGsm =
      convertCellInfoGsm_V1_2(&aCellInfo->gsm[0]);
    RefPtr<nsRilCellInfo> rilCellInfoGsm = new nsRilCellInfo(cellInfoType,
                                                             registered,
                                                             timeStampType,
                                                             timeStamp,
                                                             cellInfoGsm,
                                                             connectionStatus);
    return rilCellInfoGsm;
  } else if (aCellInfo->cellInfoType == CellInfoType::LTE) {
    RefPtr<nsCellInfoLte> cellInfoLte =
      convertCellInfoLte_V1_2(&aCellInfo->lte[0]);
    RefPtr<nsRilCellInfo> rilCellInfoLte = new nsRilCellInfo(cellInfoType,
                                                             registered,
                                                             timeStampType,
                                                             timeStamp,
                                                             cellInfoLte,
                                                             connectionStatus);
    return rilCellInfoLte;
  } else if (aCellInfo->cellInfoType == CellInfoType::WCDMA) {
    RefPtr<nsCellInfoWcdma> cellInfoWcdma =
      convertCellInfoWcdma_V1_2(&aCellInfo->wcdma[0]);
    RefPtr<nsRilCellInfo> rilCellInfoWcdma =
      new nsRilCellInfo(cellInfoType,
                        registered,
                        timeStampType,
                        timeStamp,
                        cellInfoWcdma,
                        connectionStatus);
    return rilCellInfoWcdma;
  } else if (aCellInfo->cellInfoType == CellInfoType::TD_SCDMA) {
    RefPtr<nsCellInfoTdScdma> cellInfoTdScdma =
      convertCellInfoTdScdma_V1_2(&aCellInfo->tdscdma[0]);
    RefPtr<nsRilCellInfo> rilCellInfoTdScdma =
      new nsRilCellInfo(cellInfoType,
                        registered,
                        timeStampType,
                        timeStamp,
                        cellInfoTdScdma,
                        connectionStatus);
    return rilCellInfoTdScdma;
  } else if (aCellInfo->cellInfoType == CellInfoType::CDMA) {
    RefPtr<nsCellInfoCdma> cellInfoCdma =
      convertCellInfoCdma_V1_2(&aCellInfo->cdma[0]);
    RefPtr<nsRilCellInfo> rilCellInfoCdma = new nsRilCellInfo(cellInfoType,
                                                              registered,
                                                              timeStampType,
                                                              timeStamp,
                                                              cellInfoCdma,
                                                              connectionStatus);
    return rilCellInfoCdma;
  } else {
    return nullptr;
  }
}

RefPtr<nsRilCellInfo>
nsRilResult::convertRilCellInfo_V1_4(const IRadioCellInfo_V1_4* aCellInfo)
{
  int32_t cellInfoType = (int32_t)aCellInfo->info.getDiscriminator();
  bool registered = aCellInfo->isRegistered;
  int32_t connectionStatus =
    convertConnectionStatus(aCellInfo->connectionStatus);

  if (cellInfoType == (int32_t)hidl_discriminator_V1_4::gsm) {
    RefPtr<nsCellInfoGsm> cellInfoGsm =
      convertCellInfoGsm_V1_2(&aCellInfo->info.gsm());
    RefPtr<nsRilCellInfo> rilCellInfoGsm = new nsRilCellInfo(
      cellInfoType, registered, -1, 0, cellInfoGsm, connectionStatus);
    return rilCellInfoGsm;
  } else if (cellInfoType == (int32_t)hidl_discriminator_V1_4::lte) {
    RefPtr<nsCellInfoLte> cellInfoLte =
      convertCellInfoLte_V1_4(&aCellInfo->info.lte());
    RefPtr<nsRilCellInfo> rilCellInfoLte = new nsRilCellInfo(
      cellInfoType, registered, -1, 0, cellInfoLte, connectionStatus);
    return rilCellInfoLte;
  } else if (cellInfoType == (int32_t)hidl_discriminator_V1_4::wcdma) {
    RefPtr<nsCellInfoWcdma> cellInfoWcdma =
      convertCellInfoWcdma_V1_2(&aCellInfo->info.wcdma());
    RefPtr<nsRilCellInfo> rilCellInfoWcdma = new nsRilCellInfo(
      cellInfoType, registered, -1, 0, cellInfoWcdma, connectionStatus);
    return rilCellInfoWcdma;
  } else if (cellInfoType == (int32_t)hidl_discriminator_V1_4::tdscdma) {
    RefPtr<nsCellInfoTdScdma> cellInfoTdScdma =
      convertCellInfoTdScdma_V1_2(&aCellInfo->info.tdscdma());
    RefPtr<nsRilCellInfo> rilCellInfoTdScdma = new nsRilCellInfo(
      cellInfoType, registered, -1, 0, cellInfoTdScdma, connectionStatus);
    return rilCellInfoTdScdma;
  } else if (cellInfoType == (int32_t)hidl_discriminator_V1_4::cdma) {
    RefPtr<nsCellInfoCdma> cellInfoCdma =
      convertCellInfoCdma_V1_2(&aCellInfo->info.cdma());
    RefPtr<nsRilCellInfo> rilCellInfoCdma = new nsRilCellInfo(
      cellInfoType, registered, -1, 0, cellInfoCdma, connectionStatus);
    return rilCellInfoCdma;
  } else if (cellInfoType == (int32_t)hidl_discriminator_V1_4::nr) {
    RefPtr<nsCellInfoNr> cellInfoNr = convertCellInfoNr(&aCellInfo->info.nr());
    RefPtr<nsRilCellInfo> rilCellInfoNr = new nsRilCellInfo(
      cellInfoType, registered, -1, 0, cellInfoNr, connectionStatus);
    return rilCellInfoNr;
  } else {
    return nullptr;
  }
}
#endif
RefPtr<nsCellInfoGsm>
nsRilResult::convertCellInfoGsm(const CellInfoGsm_V1_0* aCellInfoGsm)
{
  RefPtr<nsCellIdentityGsm> cellIdentityGsm =
    convertCellIdentityGsm(&aCellInfoGsm->cellIdentityGsm);
  RefPtr<nsGsmSignalStrength> gsmSignalStrength =
    convertGsmSignalStrength(aCellInfoGsm->signalStrengthGsm);
  RefPtr<nsCellInfoGsm> cellInfoGsm =
    new nsCellInfoGsm(cellIdentityGsm, gsmSignalStrength);

  return cellInfoGsm;
}
#if ANDROID_VERSION >= 33
RefPtr<nsCellInfoGsm>
nsRilResult::convertCellInfoGsm_V1_2(const IRadioCellInfoGsm_V1_2* aCellInfoGsm)
{

  RefPtr<nsCellIdentityGsm> cellIdentityGsm =
    convertCellIdentityGsm_V1_2(&aCellInfoGsm->cellIdentityGsm);
  RefPtr<nsGsmSignalStrength> gsmSignalStrength =
    convertGsmSignalStrength(aCellInfoGsm->signalStrengthGsm);
  RefPtr<nsCellInfoGsm> cellInfoGsm =
    new nsCellInfoGsm(cellIdentityGsm, gsmSignalStrength);

  return cellInfoGsm;
}
#endif
RefPtr<nsCellInfoCdma>
nsRilResult::convertCellInfoCdma(const CellInfoCdma_V1_0* aCellInfoCdma)
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
#if ANDROID_VERSION >= 33
RefPtr<nsCellInfoCdma>
nsRilResult::convertCellInfoCdma_V1_2(
  const IRadioCellInfoCdma_V1_2* aCellInfoCdma)
{
  RefPtr<nsCellIdentityCdma> cellIdentityCdma =
    convertCellIdentityCdma_V1_2(&aCellInfoCdma->cellIdentityCdma);
  RefPtr<nsCdmaSignalStrength> cdmaSignalStrength =
    convertCdmaSignalStrength(aCellInfoCdma->signalStrengthCdma);
  RefPtr<nsEvdoSignalStrength> evdoSignalStrength =
    convertEvdoSignalStrength(aCellInfoCdma->signalStrengthEvdo);
  RefPtr<nsCellInfoCdma> cellInfoCdma = new nsCellInfoCdma(
    cellIdentityCdma, cdmaSignalStrength, evdoSignalStrength);

  return cellInfoCdma;
}
#endif
RefPtr<nsCellInfoWcdma>
nsRilResult::convertCellInfoWcdma(const CellInfoWcdma_V1_0* aCellInfoWcdma)
{
  RefPtr<nsCellIdentityWcdma> cellIdentityWcdma =
    convertCellIdentityWcdma(&aCellInfoWcdma->cellIdentityWcdma);
  RefPtr<nsWcdmaSignalStrength> wcdmaSignalStrength =
    convertWcdmaSignalStrength(aCellInfoWcdma->signalStrengthWcdma);
  RefPtr<nsCellInfoWcdma> cellInfoWcdma =
    new nsCellInfoWcdma(cellIdentityWcdma, wcdmaSignalStrength);

  return cellInfoWcdma;
}
#if ANDROID_VERSION >= 33
RefPtr<nsCellInfoWcdma>
nsRilResult::convertCellInfoWcdma_V1_2(
  const IRadioCellInfoWcdma_V1_2* aCellInfoWcdma)
{
  RefPtr<nsCellIdentityWcdma> cellIdentityWcdma =
    convertCellIdentityWcdma_V1_2(&aCellInfoWcdma->cellIdentityWcdma);
  RefPtr<nsWcdmaSignalStrength> wcdmaSignalStrength =
    convertWcdmaSignalStrength_V1_2(aCellInfoWcdma->signalStrengthWcdma);
  RefPtr<nsCellInfoWcdma> cellInfoWcdma =
    new nsCellInfoWcdma(cellIdentityWcdma, wcdmaSignalStrength);

  return cellInfoWcdma;
}
#endif
RefPtr<nsCellInfoLte>
nsRilResult::convertCellInfoLte(const CellInfoLte_V1_0* aCellInfoLte)
{
  RefPtr<nsCellIdentityLte> cellIdentityLte =
    convertCellIdentityLte(&aCellInfoLte->cellIdentityLte);
  RefPtr<nsLteSignalStrength> lteSignalStrength =
    convertLteSignalStrength(aCellInfoLte->signalStrengthLte);
  // For fallback, we set cellConfigLte as null
  RefPtr<nsCellInfoLte> cellInfoLte =
    new nsCellInfoLte(cellIdentityLte, lteSignalStrength, nullptr);

  return cellInfoLte;
}
#if ANDROID_VERSION >= 33
RefPtr<nsCellInfoLte>
nsRilResult::convertCellInfoLte_V1_2(const IRadioCellInfoLte_V1_2* aCellInfoLte)
{
  RefPtr<nsCellIdentityLte> cellIdentityLte =
    convertCellIdentityLte_V1_2(&aCellInfoLte->cellIdentityLte);
  RefPtr<nsLteSignalStrength> lteSignalStrength =
    convertLteSignalStrength(aCellInfoLte->signalStrengthLte);
  // For fallback, we set cellConfigLte as null
  RefPtr<nsCellInfoLte> cellInfoLte =
    new nsCellInfoLte(cellIdentityLte, lteSignalStrength, nullptr);

  return cellInfoLte;
}

RefPtr<nsCellInfoLte>
nsRilResult::convertCellInfoLte_V1_4(const IRadioCellInfoLte_V1_4* aCellInfoLte)
{
  RefPtr<nsCellIdentityLte> cellIdentityLte =
    convertCellIdentityLte_V1_2(&aCellInfoLte->base.cellIdentityLte);
  RefPtr<nsLteSignalStrength> lteSignalStrength =
    convertLteSignalStrength(aCellInfoLte->base.signalStrengthLte);
  RefPtr<nsCellConfigLte> cellConfigLte =
    convertCellConfigLte(aCellInfoLte->cellConfig);
  RefPtr<nsCellInfoLte> cellInfoLte =
    new nsCellInfoLte(cellIdentityLte, lteSignalStrength, cellConfigLte);

  return cellInfoLte;
}
#endif
RefPtr<nsCellInfoTdScdma>
nsRilResult::convertCellInfoTdScdma(
  const CellInfoTdscdma_V1_0* aCellInfoTdscdma)
{
  RefPtr<nsCellIdentityTdScdma> cellIdentityTdScdma =
    convertCellIdentityTdScdma(&aCellInfoTdscdma->cellIdentityTdscdma);
  RefPtr<nsTdScdmaSignalStrength> tdscdmaSignalStrength =
    convertTdScdmaSignalStrength(aCellInfoTdscdma->signalStrengthTdscdma);
  RefPtr<nsCellInfoTdScdma> cellInfoTdScdma =
    new nsCellInfoTdScdma(cellIdentityTdScdma, tdscdmaSignalStrength);

  return cellInfoTdScdma;
}
#if ANDROID_VERSION >= 33
RefPtr<nsCellInfoTdScdma>
nsRilResult::convertCellInfoTdScdma_V1_2(
  const IRadioCellInfoTdscdma_V1_2* aCellInfoTdscdma)
{
  RefPtr<nsCellIdentityTdScdma> cellIdentityTdScdma =
    convertCellIdentityTdScdma_V1_2(&aCellInfoTdscdma->cellIdentityTdscdma);
  RefPtr<nsTdScdmaSignalStrength> tdscdmaSignalStrength =
    convertTdScdmaSignalStrength_V1_2(aCellInfoTdscdma->signalStrengthTdscdma);
  RefPtr<nsCellInfoTdScdma> cellInfoTdScdma =
    new nsCellInfoTdScdma(cellIdentityTdScdma, tdscdmaSignalStrength);

  return cellInfoTdScdma;
}

RefPtr<nsCellInfoNr>
nsRilResult::convertCellInfoNr(const IRadioCellInfoNr_V1_4* aCellInfoNr)
{
  RefPtr<nsCellIdentityNr> cellIdentityNr =
    convertCellIdentityNr(&aCellInfoNr->cellidentity);
  RefPtr<nsNrSignalStrength> nrSignalStrength =
    convertNrSignalStrength(&aCellInfoNr->signalStrength);
  RefPtr<nsCellInfoNr> cellInfoNr =
    new nsCellInfoNr(cellIdentityNr, nrSignalStrength);

  return cellInfoNr;
}
#endif
RefPtr<nsSetupDataCallResult>
nsRilResult::convertDcResponse(const SetupDataCallResult_V1_0& aDcResponse)
{
  // uint32_t addrLen = aDcResponse.addresses.size();
  nsTArray<RefPtr<nsLinkAddress>> linkAddresses;
  RefPtr<nsLinkAddress> linkAddress = new nsLinkAddress(
    NS_ConvertUTF8toUTF16(aDcResponse.addresses.c_str()), 0, -1, -1);
  linkAddresses.AppendElement(linkAddress);
  RefPtr<nsSetupDataCallResult> dcResponse = new nsSetupDataCallResult(
    convertDataCallFailCause(aDcResponse.status),
    aDcResponse.suggestedRetryTime,
    aDcResponse.cid,
    aDcResponse.active,
    NS_ConvertUTF8toUTF16(aDcResponse.type.c_str()),
    NS_ConvertUTF8toUTF16(aDcResponse.ifname.c_str()),
    linkAddresses,
    NS_ConvertUTF8toUTF16(aDcResponse.dnses.c_str()),
    NS_ConvertUTF8toUTF16(aDcResponse.gateways.c_str()),
    NS_ConvertUTF8toUTF16(aDcResponse.pcscf.c_str()),
    aDcResponse.mtu,
    aDcResponse.mtu,
    aDcResponse.mtu);

  return dcResponse;
}

int32_t
nsRilResult::convertCellInfoType(CellInfoType type)
{
  switch (type) {
    case CellInfoType::NONE:
      return nsICellInfoType::RADIO_CELL_INFO_TYPE_UNKNOW;
    case CellInfoType::GSM:
      return nsICellInfoType::RADIO_CELL_INFO_TYPE_GSM;
    case CellInfoType::CDMA:
      return nsICellInfoType::RADIO_CELL_INFO_TYPE_CDMA;
    case CellInfoType::LTE:
      return nsICellInfoType::RADIO_CELL_INFO_TYPE_LTE;
    case CellInfoType::WCDMA:
      return nsICellInfoType::RADIO_CELL_INFO_TYPE_WCDMA;
    case CellInfoType::TD_SCDMA:
      return nsICellInfoType::RADIO_CELL_INFO_TYPE_TD_SCDMA;
    default:
      return nsICellInfoType::RADIO_CELL_INFO_TYPE_UNKNOW;
  }
}

int32_t
nsRilResult::convertTimeStampType(TimeStampType type)
{
  switch (type) {
    case TimeStampType::UNKNOWN:
      return nsITimeStampType::RADIO_TIME_STAMP_TYPE_UNKNOW;
    case TimeStampType::ANTENNA:
      return nsITimeStampType::RADIO_TIME_STAMP_TYPE_ANTENNA;
    case TimeStampType::MODEM:
      return nsITimeStampType::RADIO_TIME_STAMP_TYPE_MODEM;
    case TimeStampType::OEM_RIL:
      return nsITimeStampType::RADIO_TIME_STAMP_TYPE_OEM_RIL;
    case TimeStampType::JAVA_RIL:
      return nsITimeStampType::RADIO_TIME_STAMP_TYPE_JAVA_RIL;
    default:
      return nsITimeStampType::RADIO_TIME_STAMP_TYPE_UNKNOW;
  }
}
#if ANDROID_VERSION >= 33
int32_t
nsRilResult::convertConnectionStatus(CellConnectionStatus_V1_2 status)
{
  switch (status) {
    case CellConnectionStatus_V1_2::NONE:
      return nsICellConnectionStatus::RADIO_CELL_CONNECTION_STATUS_NONE;
    case CellConnectionStatus_V1_2::PRIMARY_SERVING:
      return nsICellConnectionStatus::
        RADIO_CELL_CONNECTION_STATUS_PRIMARY_SERVING;
    case CellConnectionStatus_V1_2::SECONDARY_SERVING:
      return nsICellConnectionStatus::
        RADIO_CELL_CONNECTION_STATUS_SECONDARY_SERVING;
    default:
      return -1;
  }
}

int32_t
nsRilResult::convertAudioQuality(AudioQuality_V1_2 aType)
{
  switch (aType) {
    case AudioQuality_V1_2::AMR:
      return nsIAudioQuality::RADIO_CALL_AUDIOQUALITY_AMR;
    case AudioQuality_V1_2::AMR_WB:
      return nsIAudioQuality::RADIO_CALL_AUDIOQUALITY_AMR_WB;
    case AudioQuality_V1_2::GSM_EFR:
      return nsIAudioQuality::RADIO_CALL_AUDIOQUALITY_GSM_EFR;
    case AudioQuality_V1_2::GSM_FR:
      return nsIAudioQuality::RADIO_CALL_AUDIOQUALITY_GSM_FR;
    case AudioQuality_V1_2::GSM_HR:
      return nsIAudioQuality::RADIO_CALL_AUDIOQUALITY_GSM_HR;
    case AudioQuality_V1_2::EVRC:
      return nsIAudioQuality::RADIO_CALL_AUDIOQUALITY_EVRC;
    case AudioQuality_V1_2::EVRC_B:
      return nsIAudioQuality::RADIO_CALL_AUDIOQUALITY_EVRC_B;
    case AudioQuality_V1_2::EVRC_WB:
      return nsIAudioQuality::RADIO_CALL_AUDIOQUALITY_EVRC_WB;
    case AudioQuality_V1_2::EVRC_NW:
      return nsIAudioQuality::RADIO_CALL_AUDIOQUALITY_EVRC_NW;
    default:
      return nsIAudioQuality::RADIO_CALL_AUDIOQUALITY_UNSPECIFIED;
  }
}

RefPtr<nsLteVopsInfo>
nsRilResult::convertVopsInfo(const LteVopsInfo_V1_4* aVopsInfo)
{
  RefPtr<nsLteVopsInfo> vopsInfo = new nsLteVopsInfo(
    aVopsInfo->isVopsSupported, aVopsInfo->isEmcBearerSupported);
  return vopsInfo;
}

RefPtr<nsNrIndicators>
nsRilResult::convertNrIndicators(const NrIndicators_V1_4* aNrIndicators)
{
  RefPtr<nsNrIndicators> nrIndicators =
    new nsNrIndicators(aNrIndicators->isEndcAvailable,
                       aNrIndicators->isDcNrRestricted,
                       aNrIndicators->isNrAvailable);
  return nrIndicators;
}

RefPtr<nsSetupDataCallResult>
nsRilResult::convertDcResponse_V1_4(const SetupDataCallResult_V1_4& aDcResponse)
{
  uint32_t addrLen = aDcResponse.addresses.size();
  nsTArray<RefPtr<nsLinkAddress>> linkAddresses;
  for (uint32_t i = 0; i < addrLen; i++) {
    RefPtr<nsLinkAddress> linkAddress = new nsLinkAddress(
      NS_ConvertUTF8toUTF16(aDcResponse.addresses[i].c_str()), 0, -1, -1);
    linkAddresses.AppendElement(linkAddress);
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

  RefPtr<nsSetupDataCallResult> dcResponse = new nsSetupDataCallResult(
    convertDataCallFailCause_1_4(aDcResponse.cause),
    aDcResponse.suggestedRetryTime,
    aDcResponse.cid,
    (int32_t) static_cast<int>(aDcResponse.active),
    convertPdpProtocolType_V1_4((int32_t)aDcResponse.type),
    NS_ConvertUTF8toUTF16(aDcResponse.ifname.c_str()),
    linkAddresses,
    NS_ConvertUTF8toUTF16(dnses),
    NS_ConvertUTF8toUTF16(gateways),
    NS_ConvertUTF8toUTF16(pcscf),
    aDcResponse.mtu,
    aDcResponse.mtu,
    aDcResponse.mtu);

  return dcResponse;
}
RefPtr<nsSetupDataCallResult>
nsRilResult::convertDcResponse_V1_5(const SetupDataCallResult_V1_5& aDcResponse)
{
  nsTArray<RefPtr<nsLinkAddress>> addresses;
  uint32_t addrLen = aDcResponse.addresses.size();
  for (uint32_t i = 0; i < addrLen; i++) {
    RefPtr<nsLinkAddress> linkAddress =
      convertLinkAddress_V1_5(aDcResponse.addresses[i]);
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
  RefPtr<nsSetupDataCallResult> dcResponse = new nsSetupDataCallResult(
    convertDataCallFailCause_1_4(aDcResponse.cause),
    aDcResponse.suggestedRetryTime,
    aDcResponse.cid,
    (int32_t) static_cast<int>(aDcResponse.active),
    convertPdpProtocolType_V1_4((int32_t)aDcResponse.type),
    NS_ConvertUTF8toUTF16(aDcResponse.ifname.c_str()),
    addresses,
    NS_ConvertUTF8toUTF16(dnses),
    NS_ConvertUTF8toUTF16(gateways),
    NS_ConvertUTF8toUTF16(pcscf),
    mtu,
    aDcResponse.mtuV4,
    aDcResponse.mtuV6);

  return dcResponse;
}

RefPtr<nsLinkAddress>
nsRilResult::convertLinkAddress_V1_5(const LinkAddress_V1_5& aLinkaddress)
{
  RefPtr<nsLinkAddress> linkAddress =
    new nsLinkAddress(NS_ConvertUTF8toUTF16(aLinkaddress.address.c_str()),
                      (int32_t)aLinkaddress.properties,
                      aLinkaddress.deprecationTime,
                      aLinkaddress.expirationTime);
  return linkAddress;
}

RefPtr<nsCellIdentityGsm>
nsRilResult::convertCellIdentityGsm_V1_5(
  const CellIdentityGsm_V1_5* aCellIdentityGsm)
{
  nsString mcc = NS_ConvertUTF8toUTF16(aCellIdentityGsm->base.base.mcc.c_str());
  nsString mnc = NS_ConvertUTF8toUTF16(aCellIdentityGsm->base.base.mnc.c_str());
  int32_t lac = aCellIdentityGsm->base.base.lac;
  int32_t cid = aCellIdentityGsm->base.base.cid;
  int32_t arfcn = aCellIdentityGsm->base.base.arfcn;
  int32_t bsic = aCellIdentityGsm->base.base.bsic;
  RefPtr<nsCellIdentityOperatorNames> operatorNames =
    convertCellIdentityOperatorNames(aCellIdentityGsm->base.operatorNames);
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
nsRilResult::convertCsgInfo(const OptionalCsgInfo_V1_5* aCsgInfo)
{
  if ((int32_t)aCsgInfo->getDiscriminator() == 1) {
    RefPtr<nsCellIdentityCsgInfo> csgInfo = new nsCellIdentityCsgInfo(
      aCsgInfo->csgInfo().csgIndication,
      NS_ConvertUTF8toUTF16(aCsgInfo->csgInfo().homeNodebName.c_str()),
      aCsgInfo->csgInfo().csgIdentity);
    return csgInfo;
  }
  return nullptr;
}

RefPtr<nsCellIdentityLte>
nsRilResult::convertCellIdentityLte_V1_5(
  const CellIdentityLte_V1_5* aCellIdentityLte)
{
  nsString mcc;
  nsString mnc;
  int32_t ci;
  int32_t pci;
  int32_t tac;
  int32_t earfcn;
  int32_t bandwidth = 0;
  RefPtr<nsCellIdentityOperatorNames> operatorNames;

  mcc = NS_ConvertUTF8toUTF16(aCellIdentityLte->base.base.mcc.c_str());
  mnc = NS_ConvertUTF8toUTF16(aCellIdentityLte->base.base.mnc.c_str());
  ci = aCellIdentityLte->base.base.ci;
  pci = aCellIdentityLte->base.base.pci;
  tac = aCellIdentityLte->base.base.tac;
  earfcn = aCellIdentityLte->base.base.earfcn;
  operatorNames =
    convertCellIdentityOperatorNames(aCellIdentityLte->base.operatorNames);
  bandwidth = aCellIdentityLte->base.bandwidth;

  nsTArray<nsString> addlPlmns;
  uint32_t adPlmnLen = aCellIdentityLte->additionalPlmns.size();
  for (uint32_t i = 0; i < adPlmnLen; i++) {
    nsString addPlmn;
    addPlmn.Assign(
      NS_ConvertUTF8toUTF16(aCellIdentityLte->additionalPlmns[i].c_str()));
    addlPlmns.AppendElement(addPlmn);
  }
  RefPtr<nsCellIdentityCsgInfo> csgInfo =
    convertCsgInfo(&aCellIdentityLte->optionalCsgInfo);
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
nsRilResult::convertCellIdentityWcdma_V1_5(
  const CellIdentityWcdma_V1_5* aCellIdentityWcdma)
{
  nsString mcc =
    NS_ConvertUTF8toUTF16(aCellIdentityWcdma->base.base.mcc.c_str());
  nsString mnc =
    NS_ConvertUTF8toUTF16(aCellIdentityWcdma->base.base.mnc.c_str());
  int32_t lac = aCellIdentityWcdma->base.base.lac;
  int32_t cid = aCellIdentityWcdma->base.base.cid;
  int32_t psc = aCellIdentityWcdma->base.base.psc;
  int32_t uarfcn = aCellIdentityWcdma->base.base.uarfcn;
  RefPtr<nsCellIdentityOperatorNames> operatorNames =
    convertCellIdentityOperatorNames(aCellIdentityWcdma->base.operatorNames);
  nsTArray<nsString> addlPlmns;
  uint32_t adPlmnLen = aCellIdentityWcdma->additionalPlmns.size();
  for (uint32_t i = 0; i < adPlmnLen; i++) {
    nsString addPlmn;
    addPlmn.Assign(
      NS_ConvertUTF8toUTF16(aCellIdentityWcdma->additionalPlmns[i].c_str()));
    addlPlmns.AppendElement(addPlmn);
  }
  RefPtr<nsCellIdentityCsgInfo> csgInfo =
    convertCsgInfo(&aCellIdentityWcdma->optionalCsgInfo);

  RefPtr<nsCellIdentityWcdma> wcdma = new nsCellIdentityWcdma(
    mcc, mnc, lac, cid, psc, uarfcn, operatorNames, addlPlmns, csgInfo);
  return wcdma;
}

RefPtr<nsCellIdentityTdScdma>
nsRilResult::convertCellIdentityTdScdma_V1_5(
  const CellIdentityTdscdma_V1_5* aCellIdentityTdScdma)
{
  nsString mcc =
    NS_ConvertUTF8toUTF16(aCellIdentityTdScdma->base.base.mcc.c_str());
  nsString mnc =
    NS_ConvertUTF8toUTF16(aCellIdentityTdScdma->base.base.mnc.c_str());
  int32_t lac = aCellIdentityTdScdma->base.base.lac;
  int32_t cid = aCellIdentityTdScdma->base.base.cid;
  int32_t cpid = aCellIdentityTdScdma->base.base.cpid;
  int32_t uarfcn = aCellIdentityTdScdma->base.uarfcn;
  RefPtr<nsCellIdentityOperatorNames> operatorNames =
    convertCellIdentityOperatorNames(aCellIdentityTdScdma->base.operatorNames);

  nsTArray<nsString> addlPlmns;
  uint32_t adPlmnLen = aCellIdentityTdScdma->additionalPlmns.size();
  for (uint32_t i = 0; i < adPlmnLen; i++) {
    nsString addPlmn;
    addPlmn.Assign(
      NS_ConvertUTF8toUTF16(aCellIdentityTdScdma->additionalPlmns[i].c_str()));
    addlPlmns.AppendElement(addPlmn);
  }
  RefPtr<nsCellIdentityCsgInfo> csgInfo =
    convertCsgInfo(&aCellIdentityTdScdma->optionalCsgInfo);

  RefPtr<nsCellIdentityTdScdma> tdscdma = new nsCellIdentityTdScdma(
    mcc, mnc, lac, cid, cpid, operatorNames, uarfcn, addlPlmns, csgInfo);
  return tdscdma;
}

RefPtr<nsCellIdentityNr>
nsRilResult::convertCellIdentityNr_V1_5(
  const CellIdentityNr_V1_5* aCellIdentityNr)
{
  nsString mcc = NS_ConvertUTF8toUTF16(aCellIdentityNr->base.mcc.c_str());
  nsString mnc = NS_ConvertUTF8toUTF16(aCellIdentityNr->base.mnc.c_str());
  RefPtr<nsCellIdentityOperatorNames> operatorNames =
    convertCellIdentityOperatorNames(aCellIdentityNr->base.operatorNames);

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

  RefPtr<nsCellIdentityNr> nr =
    new nsCellIdentityNr(mcc,
                         mnc,
                         aCellIdentityNr->base.nci,
                         aCellIdentityNr->base.pci,
                         aCellIdentityNr->base.tac,
                         aCellIdentityNr->base.nrarfcn,
                         operatorNames,
                         addlPlmns,
                         bands);
  return nr;
}

int32_t
nsRilResult::convertCellTypeToRil(uint8_t type)
{
  switch (type) {
    case (uint8_t)CellType_V1_5::noinit:
      return nsICellInfoType::RADIO_CELL_INFO_TYPE_UNKNOW;
    case (uint8_t)CellType_V1_5::gsm:
      return nsICellInfoType::RADIO_CELL_INFO_TYPE_GSM;
    case (uint8_t)CellType_V1_5::cdma:
      return nsICellInfoType::RADIO_CELL_INFO_TYPE_CDMA;
    case (uint8_t)CellType_V1_5::lte:
      return nsICellInfoType::RADIO_CELL_INFO_TYPE_LTE;
    case (uint8_t)CellType_V1_5::wcdma:
      return nsICellInfoType::RADIO_CELL_INFO_TYPE_WCDMA;
    case (uint8_t)CellType_V1_5::tdscdma:
      return nsICellInfoType::RADIO_CELL_INFO_TYPE_TD_SCDMA;
    case (uint8_t)CellType_V1_5::nr:
      return nsICellInfoType::RADIO_CELL_INFO_TYPE_NR;
    default:
      return nsICellInfoType::RADIO_CELL_INFO_TYPE_UNKNOW;
  }
}

int32_t
nsRilResult::convertCellInfoTypeToRil(uint8_t type)
{
  switch (type) {
    case (uint8_t)CellInfoType_V1_5::gsm:
      return nsICellInfoType::RADIO_CELL_INFO_TYPE_GSM;
    case (uint8_t)CellInfoType_V1_5::cdma:
      return nsICellInfoType::RADIO_CELL_INFO_TYPE_CDMA;
    case (uint8_t)CellInfoType_V1_5::lte:
      return nsICellInfoType::RADIO_CELL_INFO_TYPE_LTE;
    case (uint8_t)CellInfoType_V1_5::wcdma:
      return nsICellInfoType::RADIO_CELL_INFO_TYPE_WCDMA;
    case (uint8_t)CellInfoType_V1_5::tdscdma:
      return nsICellInfoType::RADIO_CELL_INFO_TYPE_TD_SCDMA;
    case (uint8_t)CellInfoType_V1_5::nr:
      return nsICellInfoType::RADIO_CELL_INFO_TYPE_NR;
    default:
      return nsICellInfoType::RADIO_CELL_INFO_TYPE_UNKNOW;
  }
}

RefPtr<nsCellIdentity>
nsRilResult::convertCellIdentity_V1_5(const CellIdentity_V1_5* aCellIdentity)
{
  int32_t cellInfoType =
    convertCellTypeToRil((uint8_t)aCellIdentity->getDiscriminator());
  if (cellInfoType == nsICellInfoType::RADIO_CELL_INFO_TYPE_GSM) {
    RefPtr<nsCellIdentityGsm> cellIdentityGsm =
      convertCellIdentityGsm_V1_5(&(aCellIdentity->gsm()));
    RefPtr<nsCellIdentity> cellIdentity =
      new nsCellIdentity(cellInfoType, cellIdentityGsm);
    return cellIdentity;
  } else if (cellInfoType == nsICellInfoType::RADIO_CELL_INFO_TYPE_LTE) {
    RefPtr<nsCellIdentityLte> cellIdentityLte =
      convertCellIdentityLte_V1_5(&(aCellIdentity->lte()));
    RefPtr<nsCellIdentity> cellIdentity =
      new nsCellIdentity(cellInfoType, cellIdentityLte);
    return cellIdentity;
  } else if (cellInfoType == nsICellInfoType::RADIO_CELL_INFO_TYPE_WCDMA) {
    RefPtr<nsCellIdentityWcdma> cellIdentityWcdma =
      convertCellIdentityWcdma_V1_5(&(aCellIdentity->wcdma()));
    RefPtr<nsCellIdentity> cellIdentity =
      new nsCellIdentity(cellInfoType, cellIdentityWcdma);
    return cellIdentity;
  } else if (cellInfoType == nsICellInfoType::RADIO_CELL_INFO_TYPE_TD_SCDMA) {
    RefPtr<nsCellIdentityTdScdma> cellIdentityTdScdma =
      convertCellIdentityTdScdma_V1_5(&(aCellIdentity->tdscdma()));
    RefPtr<nsCellIdentity> cellIdentity =
      new nsCellIdentity(cellInfoType, cellIdentityTdScdma);
    return cellIdentity;
  } else if (cellInfoType == nsICellInfoType::RADIO_CELL_INFO_TYPE_CDMA) {
    RefPtr<nsCellIdentityCdma> cellIdentityCdma =
      convertCellIdentityCdma_V1_2(&(aCellIdentity->cdma()));
    RefPtr<nsCellIdentity> cellIdentity =
      new nsCellIdentity(cellInfoType, cellIdentityCdma);
    return cellIdentity;
  } else if (cellInfoType == nsICellInfoType::RADIO_CELL_INFO_TYPE_NR) {
    RefPtr<nsCellIdentityNr> cellIdentityNr =
      convertCellIdentityNr_V1_5(&(aCellIdentity->nr()));
    RefPtr<nsCellIdentity> cellIdentity =
      new nsCellIdentity(cellInfoType, cellIdentityNr);
    return cellIdentity;
  }
  return nullptr;
}

RefPtr<nsCellInfoGsm>
nsRilResult::convertCellInfoGsm_V1_5(const CellInfoGsm_V1_5* aCellInfoGsm)
{

  RefPtr<nsCellIdentityGsm> cellIdentityGsm =
    convertCellIdentityGsm_V1_5(&aCellInfoGsm->cellIdentityGsm);
  RefPtr<nsGsmSignalStrength> gsmSignalStrength =
    convertGsmSignalStrength(aCellInfoGsm->signalStrengthGsm);
  RefPtr<nsCellInfoGsm> cellInfoGsm =
    new nsCellInfoGsm(cellIdentityGsm, gsmSignalStrength);

  return cellInfoGsm;
}

RefPtr<nsCellInfoWcdma>
nsRilResult::convertCellInfoWcdma_V1_5(const CellInfoWcdma_V1_5* aCellInfoWcdma)
{
  RefPtr<nsCellIdentityWcdma> cellIdentityWcdma =
    convertCellIdentityWcdma_V1_5(&aCellInfoWcdma->cellIdentityWcdma);
  RefPtr<nsWcdmaSignalStrength> wcdmaSignalStrength =
    convertWcdmaSignalStrength_V1_2(aCellInfoWcdma->signalStrengthWcdma);
  RefPtr<nsCellInfoWcdma> cellInfoWcdma =
    new nsCellInfoWcdma(cellIdentityWcdma, wcdmaSignalStrength);

  return cellInfoWcdma;
}

RefPtr<nsCellInfoTdScdma>
nsRilResult::convertCellInfoTdScdma_V1_5(
  const CellInfoTdscdma_V1_5* aCellInfoTdscdma)
{
  RefPtr<nsCellIdentityTdScdma> cellIdentityTdScdma =
    convertCellIdentityTdScdma_V1_5(&aCellInfoTdscdma->cellIdentityTdscdma);
  RefPtr<nsTdScdmaSignalStrength> tdscdmaSignalStrength =
    convertTdScdmaSignalStrength_V1_2(aCellInfoTdscdma->signalStrengthTdscdma);
  RefPtr<nsCellInfoTdScdma> cellInfoTdScdma =
    new nsCellInfoTdScdma(cellIdentityTdScdma, tdscdmaSignalStrength);

  return cellInfoTdScdma;
}

RefPtr<nsCellInfoLte>
nsRilResult::convertCellInfoLte_V1_5(const CellInfoLte_V1_5* aCellInfoLte)
{
  RefPtr<nsCellIdentityLte> cellIdentityLte =
    convertCellIdentityLte_V1_5(&aCellInfoLte->cellIdentityLte);
  RefPtr<nsLteSignalStrength> lteSignalStrength =
    convertLteSignalStrength(aCellInfoLte->signalStrengthLte);
  RefPtr<nsCellConfigLte> cellConfigLte = nullptr;
  RefPtr<nsCellInfoLte> cellInfoLte =
    new nsCellInfoLte(cellIdentityLte, lteSignalStrength, cellConfigLte);

  return cellInfoLte;
}

RefPtr<nsCellInfoNr>
nsRilResult::convertCellInfoNr_V1_5(const CellInfoNr_V1_5* aCellInfoNr)
{
  RefPtr<nsCellIdentityNr> cellIdentityNr =
    convertCellIdentityNr_V1_5(&aCellInfoNr->cellIdentityNr);
  RefPtr<nsNrSignalStrength> nrSignalStrength =
    convertNrSignalStrength(&aCellInfoNr->signalStrengthNr);
  RefPtr<nsCellInfoNr> cellInfoNr =
    new nsCellInfoNr(cellIdentityNr, nrSignalStrength);

  return cellInfoNr;
}

RefPtr<nsRilCellInfo>
nsRilResult::convertRilCellInfo_V1_5(const CellInfo_V1_5* aCellInfo)
{
  int32_t cellInfoType = convertCellInfoTypeToRil(
    (uint8_t)aCellInfo->ratSpecificInfo.getDiscriminator());
  bool registered = aCellInfo->registered;
  int32_t connectionStatus =
    convertConnectionStatus(aCellInfo->connectionStatus);
  int32_t timeStampType = convertTimeStampType(aCellInfo->timeStampType);
  uint64_t timeStamp = aCellInfo->timeStamp;

  if (cellInfoType == nsICellInfoType::RADIO_CELL_INFO_TYPE_GSM) {
    RefPtr<nsCellInfoGsm> cellInfoGsm =
      convertCellInfoGsm_V1_5(&aCellInfo->ratSpecificInfo.gsm());
    RefPtr<nsRilCellInfo> rilCellInfoGsm = new nsRilCellInfo(cellInfoType,
                                                             registered,
                                                             timeStampType,
                                                             timeStamp,
                                                             cellInfoGsm,
                                                             connectionStatus);
    return rilCellInfoGsm;
  } else if (cellInfoType == nsICellInfoType::RADIO_CELL_INFO_TYPE_LTE) {
    RefPtr<nsCellInfoLte> cellInfoLte =
      convertCellInfoLte_V1_5(&aCellInfo->ratSpecificInfo.lte());
    RefPtr<nsRilCellInfo> rilCellInfoLte = new nsRilCellInfo(cellInfoType,
                                                             registered,
                                                             timeStampType,
                                                             timeStamp,
                                                             cellInfoLte,
                                                             connectionStatus);
    return rilCellInfoLte;
  } else if (cellInfoType == nsICellInfoType::RADIO_CELL_INFO_TYPE_WCDMA) {
    RefPtr<nsCellInfoWcdma> cellInfoWcdma =
      convertCellInfoWcdma_V1_5(&aCellInfo->ratSpecificInfo.wcdma());
    RefPtr<nsRilCellInfo> rilCellInfoWcdma =
      new nsRilCellInfo(cellInfoType,
                        registered,
                        timeStampType,
                        timeStamp,
                        cellInfoWcdma,
                        connectionStatus);
    return rilCellInfoWcdma;
  } else if (cellInfoType == nsICellInfoType::RADIO_CELL_INFO_TYPE_TD_SCDMA) {
    RefPtr<nsCellInfoTdScdma> cellInfoTdScdma =
      convertCellInfoTdScdma_V1_5(&aCellInfo->ratSpecificInfo.tdscdma());
    RefPtr<nsRilCellInfo> rilCellInfoTdScdma =
      new nsRilCellInfo(cellInfoType,
                        registered,
                        timeStampType,
                        timeStamp,
                        cellInfoTdScdma,
                        connectionStatus);
    return rilCellInfoTdScdma;
  } else if (cellInfoType == nsICellInfoType::RADIO_CELL_INFO_TYPE_CDMA) {
    RefPtr<nsCellInfoCdma> cellInfoCdma =
      convertCellInfoCdma_V1_2(&aCellInfo->ratSpecificInfo.cdma());
    RefPtr<nsRilCellInfo> rilCellInfoCdma = new nsRilCellInfo(cellInfoType,
                                                              registered,
                                                              timeStampType,
                                                              timeStamp,
                                                              cellInfoCdma,
                                                              connectionStatus);
    return rilCellInfoCdma;
  } else if (cellInfoType == nsICellInfoType::RADIO_CELL_INFO_TYPE_NR) {
    RefPtr<nsCellInfoNr> cellInfoNr =
      convertCellInfoNr_V1_5(&aCellInfo->ratSpecificInfo.nr());
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

int32_t
nsRilResult::convertDataCallFailCause_1_6(DataCallFailCause_V1_6 aCause)
{
  switch (aCause) {
    case DataCallFailCause_V1_6::NONE:
      return nsIDataCallFailCause::DATACALL_FAIL_NONE;
    case DataCallFailCause_V1_6::OPERATOR_BARRED:
      return nsIDataCallFailCause::DATACALL_FAIL_OPERATOR_BARRED;
    case DataCallFailCause_V1_6::NAS_SIGNALLING:
      return nsIDataCallFailCause::DATACALL_FAIL_NAS_SIGNALLING;
    case DataCallFailCause_V1_6::INSUFFICIENT_RESOURCES:
      return nsIDataCallFailCause::DATACALL_FAIL_INSUFFICIENT_RESOURCES;
    case DataCallFailCause_V1_6::MISSING_UKNOWN_APN:
      return nsIDataCallFailCause::DATACALL_FAIL_MISSING_UKNOWN_APN;
    case DataCallFailCause_V1_6::UNKNOWN_PDP_ADDRESS_TYPE:
      return nsIDataCallFailCause::DATACALL_FAIL_UNKNOWN_PDP_ADDRESS_TYPE;
    case DataCallFailCause_V1_6::USER_AUTHENTICATION:
      return nsIDataCallFailCause::DATACALL_FAIL_USER_AUTHENTICATION;
    case DataCallFailCause_V1_6::ACTIVATION_REJECT_GGSN:
      return nsIDataCallFailCause::DATACALL_FAIL_ACTIVATION_REJECT_GGSN;
    case DataCallFailCause_V1_6::ACTIVATION_REJECT_UNSPECIFIED:
      return nsIDataCallFailCause::DATACALL_FAIL_ACTIVATION_REJECT_UNSPECIFIED;
    case DataCallFailCause_V1_6::SERVICE_OPTION_NOT_SUPPORTED:
      return nsIDataCallFailCause::DATACALL_FAIL_SERVICE_OPTION_NOT_SUPPORTED;
    case DataCallFailCause_V1_6::SERVICE_OPTION_NOT_SUBSCRIBED:
      return nsIDataCallFailCause::DATACALL_FAIL_SERVICE_OPTION_NOT_SUBSCRIBED;
    case DataCallFailCause_V1_6::SERVICE_OPTION_OUT_OF_ORDER:
      return nsIDataCallFailCause::DATACALL_FAIL_SERVICE_OPTION_OUT_OF_ORDER;
    case DataCallFailCause_V1_6::NSAPI_IN_USE:
      return nsIDataCallFailCause::DATACALL_FAIL_NSAPI_IN_USE;
    case DataCallFailCause_V1_6::REGULAR_DEACTIVATION:
      return nsIDataCallFailCause::DATACALL_FAIL_REGULAR_DEACTIVATION;
    case DataCallFailCause_V1_6::QOS_NOT_ACCEPTED:
      return nsIDataCallFailCause::DATACALL_FAIL_QOS_NOT_ACCEPTED;
    case DataCallFailCause_V1_6::NETWORK_FAILURE:
      return nsIDataCallFailCause::DATACALL_FAIL_NETWORK_FAILURE;
    case DataCallFailCause_V1_6::UMTS_REACTIVATION_REQ:
      return nsIDataCallFailCause::DATACALL_FAIL_UMTS_REACTIVATION_REQ;
    case DataCallFailCause_V1_6::FEATURE_NOT_SUPP:
      return nsIDataCallFailCause::DATACALL_FAIL_FEATURE_NOT_SUPP;
    case DataCallFailCause_V1_6::TFT_SEMANTIC_ERROR:
      return nsIDataCallFailCause::DATACALL_FAIL_TFT_SEMANTIC_ERROR;
    case DataCallFailCause_V1_6::TFT_SYTAX_ERROR:
      return nsIDataCallFailCause::DATACALL_FAIL_TFT_SYTAX_ERROR;
    case DataCallFailCause_V1_6::UNKNOWN_PDP_CONTEXT:
      return nsIDataCallFailCause::DATACALL_FAIL_UNKNOWN_PDP_CONTEXT;
    case DataCallFailCause_V1_6::FILTER_SEMANTIC_ERROR:
      return nsIDataCallFailCause::DATACALL_FAIL_FILTER_SEMANTIC_ERROR;
    case DataCallFailCause_V1_6::FILTER_SYTAX_ERROR:
      return nsIDataCallFailCause::DATACALL_FAIL_FILTER_SYTAX_ERROR;
    case DataCallFailCause_V1_6::PDP_WITHOUT_ACTIVE_TFT:
      return nsIDataCallFailCause::DATACALL_FAIL_PDP_WITHOUT_ACTIVE_TFT;
    case DataCallFailCause_V1_6::ONLY_IPV4_ALLOWED:
      return nsIDataCallFailCause::DATACALL_FAIL_ONLY_IPV4_ALLOWED;
    case DataCallFailCause_V1_6::ONLY_IPV6_ALLOWED:
      return nsIDataCallFailCause::DATACALL_FAIL_ONLY_IPV6_ALLOWED;
    case DataCallFailCause_V1_6::ONLY_SINGLE_BEARER_ALLOWED:
      return nsIDataCallFailCause::DATACALL_FAIL_ONLY_SINGLE_BEARER_ALLOWED;
    case DataCallFailCause_V1_6::ESM_INFO_NOT_RECEIVED:
      return nsIDataCallFailCause::DATACALL_FAIL_ESM_INFO_NOT_RECEIVED;
    case DataCallFailCause_V1_6::PDN_CONN_DOES_NOT_EXIST:
      return nsIDataCallFailCause::DATACALL_FAIL_PDN_CONN_DOES_NOT_EXIST;
    case DataCallFailCause_V1_6::MULTI_CONN_TO_SAME_PDN_NOT_ALLOWED:
      return nsIDataCallFailCause::
        DATACALL_FAIL_MULTI_CONN_TO_SAME_PDN_NOT_ALLOWED;
    case DataCallFailCause_V1_6::MAX_ACTIVE_PDP_CONTEXT_REACHED:
      return nsIDataCallFailCause::DATACALL_FAIL_MAX_ACTIVE_PDP_CONTEXT_REACHED;
    case DataCallFailCause_V1_6::UNSUPPORTED_APN_IN_CURRENT_PLMN:
      return nsIDataCallFailCause::
        DATACALL_FAIL_UNSUPPORTED_APN_IN_CURRENT_PLMN;
    case DataCallFailCause_V1_6::INVALID_TRANSACTION_ID:
      return nsIDataCallFailCause::DATACALL_FAIL_INVALID_TRANSACTION_ID;
    case DataCallFailCause_V1_6::MESSAGE_INCORRECT_SEMANTIC:
      return nsIDataCallFailCause::DATACALL_FAIL_MESSAGE_INCORRECT_SEMANTIC;
    case DataCallFailCause_V1_6::INVALID_MANDATORY_INFO:
      return nsIDataCallFailCause::DATACALL_FAIL_INVALID_MANDATORY_INFO;
    case DataCallFailCause_V1_6::MESSAGE_TYPE_UNSUPPORTED:
      return nsIDataCallFailCause::DATACALL_FAIL_MESSAGE_TYPE_UNSUPPORTED;
    case DataCallFailCause_V1_6::MSG_TYPE_NONCOMPATIBLE_STATE:
      return nsIDataCallFailCause::DATACALL_FAIL_MSG_TYPE_NONCOMPATIBLE_STATE;
    case DataCallFailCause_V1_6::UNKNOWN_INFO_ELEMENT:
      return nsIDataCallFailCause::DATACALL_FAIL_UNKNOWN_INFO_ELEMENT;
    case DataCallFailCause_V1_6::CONDITIONAL_IE_ERROR:
      return nsIDataCallFailCause::DATACALL_FAIL_CONDITIONAL_IE_ERROR;
    case DataCallFailCause_V1_6::MSG_AND_PROTOCOL_STATE_UNCOMPATIBLE:
      return nsIDataCallFailCause::
        DATACALL_FAIL_MSG_AND_PROTOCOL_STATE_UNCOMPATIBLE;
    case DataCallFailCause_V1_6::PROTOCOL_ERRORS:
      return nsIDataCallFailCause::DATACALL_FAIL_PROTOCOL_ERRORS;
    case DataCallFailCause_V1_6::APN_TYPE_CONFLICT:
      return nsIDataCallFailCause::DATACALL_FAIL_APN_TYPE_CONFLICT;
    case DataCallFailCause_V1_6::INVALID_PCSCF_ADDR:
      return nsIDataCallFailCause::DATACALL_FAIL_INVALID_PCSCF_ADDR;
    case DataCallFailCause_V1_6::INTERNAL_CALL_PREEMPT_BY_HIGH_PRIO_APN:
      return nsIDataCallFailCause::
        DATACALL_FAIL_INTERNAL_CALL_PREEMPT_BY_HIGH_PRIO_APN;
    case DataCallFailCause_V1_6::EMM_ACCESS_BARRED:
      return nsIDataCallFailCause::DATACALL_FAIL_EMM_ACCESS_BARRED;
    case DataCallFailCause_V1_6::EMERGENCY_IFACE_ONLY:
      return nsIDataCallFailCause::DATACALL_FAIL_EMERGENCY_IFACE_ONLY;
    case DataCallFailCause_V1_6::IFACE_MISMATCH:
      return nsIDataCallFailCause::DATACALL_FAIL_IFACE_MISMATCH;
    case DataCallFailCause_V1_6::COMPANION_IFACE_IN_USE:
      return nsIDataCallFailCause::DATACALL_FAIL_COMPANION_IFACE_IN_USE;
    case DataCallFailCause_V1_6::IP_ADDRESS_MISMATCH:
      return nsIDataCallFailCause::DATACALL_FAIL_IP_ADDRESS_MISMATCH;
    case DataCallFailCause_V1_6::IFACE_AND_POL_FAMILY_MISMATCH:
      return nsIDataCallFailCause::DATACALL_FAIL_IFACE_AND_POL_FAMILY_MISMATCH;
    case DataCallFailCause_V1_6::EMM_ACCESS_BARRED_INFINITE_RETRY:
      return nsIDataCallFailCause::
        DATACALL_FAIL_EMM_ACCESS_BARRED_INFINITE_RETRY;
    case DataCallFailCause_V1_6::AUTH_FAILURE_ON_EMERGENCY_CALL:
      return nsIDataCallFailCause::DATACALL_FAIL_AUTH_FAILURE_ON_EMERGENCY_CALL;
    case DataCallFailCause_V1_6::VOICE_REGISTRATION_FAIL:
      return nsIDataCallFailCause::DATACALL_FAIL_VOICE_REGISTRATION_FAIL;
    case DataCallFailCause_V1_6::DATA_REGISTRATION_FAIL:
      return nsIDataCallFailCause::DATACALL_FAIL_DATA_REGISTRATION_FAIL;
    case DataCallFailCause_V1_6::SIGNAL_LOST:
      return nsIDataCallFailCause::DATACALL_FAIL_SIGNAL_LOST;
    case DataCallFailCause_V1_6::PREF_RADIO_TECH_CHANGED:
      return nsIDataCallFailCause::DATACALL_FAIL_PREF_RADIO_TECH_CHANGED;
    case DataCallFailCause_V1_6::RADIO_POWER_OFF:
      return nsIDataCallFailCause::DATACALL_FAIL_RADIO_POWER_OFF;
    case DataCallFailCause_V1_6::TETHERED_CALL_ACTIVE:
      return nsIDataCallFailCause::DATACALL_FAIL_TETHERED_CALL_ACTIVE;
    case DataCallFailCause_V1_6::ERROR_UNSPECIFIED:
      return nsIDataCallFailCause::DATACALL_FAIL_ERROR_UNSPECIFIED;
    case DataCallFailCause_V1_6::LLC_SNDCP:
      return nsIDataCallFailCause::DATACALL_FAIL_LLC_SNDCP;
    default:
      return nsIDataCallFailCause::DATACALL_FAIL_ERROR_UNSPECIFIED;
  }
}

RefPtr<nsSetupDataCallResult>
nsRilResult::convertDcResponse_V1_6(const SetupDataCallResult_V1_6& aDcResponse)
{
  nsTArray<RefPtr<nsLinkAddress>> addresses;
  uint32_t addrLen = aDcResponse.addresses.size();
  for (uint32_t i = 0; i < addrLen; i++) {
    RefPtr<nsLinkAddress> linkAddress =
      convertLinkAddress_V1_5(aDcResponse.addresses[i]);
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
  if (aDcResponse.sliceInfo.getDiscriminator() ==
      OptionalSliceInfo_V1_6::hidl_discriminator::value) {
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

  RefPtr<nsSetupDataCallResult> dcResponse = new nsSetupDataCallResult(
    convertDataCallFailCause_1_6(aDcResponse.cause),
    aDcResponse.suggestedRetryTime,
    aDcResponse.cid,
    (int32_t) static_cast<int>(aDcResponse.active),
    convertPdpProtocolType_V1_4((int32_t)aDcResponse.type),
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

RefPtr<nsNrVopsInfo>
nsRilResult::convertNrVopsInfo(const NrVopsInfo* aNrVopsInfo)
{
  RefPtr<nsNrVopsInfo> vopsInfo =
    new nsNrVopsInfo((int32_t)aNrVopsInfo->vopsSupported,
                     (int32_t)aNrVopsInfo->emcSupported,
                     (int32_t)aNrVopsInfo->emfSupported);
  return vopsInfo;
}

RefPtr<nsLteSignalStrength>
nsRilResult::convertLteSignalStrength_V1_6(
  const LteSignalStrength_V1_6& aLteSignalStrength)
{
  RefPtr<nsLteSignalStrength> lteSignalStrength =
    new nsLteSignalStrength(aLteSignalStrength.base.signalStrength,
                            aLteSignalStrength.base.rsrp,
                            aLteSignalStrength.base.rsrq,
                            aLteSignalStrength.base.rssnr,
                            aLteSignalStrength.base.cqi,
                            aLteSignalStrength.base.timingAdvance,
                            aLteSignalStrength.cqiTableIndex);

  return lteSignalStrength;
}

RefPtr<nsNrSignalStrength>
nsRilResult::convertNrSignalStrength_V1_6(
  const NrSignalStrength_V1_6* aNrSignalStrength)
{
  nsTArray<int32_t> csiCqiReport;
  uint32_t csiLen = aNrSignalStrength->csiCqiReport.size();
  for (uint32_t i = 0; i < csiLen; i++) {
    int32_t cqi = (int32_t)aNrSignalStrength->csiCqiReport[i];
    csiCqiReport.AppendElement(cqi);
  }
  RefPtr<nsNrSignalStrength> nrSignalStrength =
    new nsNrSignalStrength(aNrSignalStrength->base.ssRsrp,
                           aNrSignalStrength->base.ssRsrq,
                           aNrSignalStrength->base.ssSinr,
                           aNrSignalStrength->base.csiRsrp,
                           aNrSignalStrength->base.csiRsrq,
                           aNrSignalStrength->base.csiSinr,
                           aNrSignalStrength->csiCqiTableIndex,
                           csiCqiReport);

  return nrSignalStrength;
}

RefPtr<nsSignalStrength>
nsRilResult::convertSignalStrength_V1_6(
  const ISignalStrength_V1_6& aSignalStrength)
{
  RefPtr<nsGsmSignalStrength> gsmSignalStrength =
    convertGsmSignalStrength(aSignalStrength.gsm);
  RefPtr<nsCdmaSignalStrength> cdmaSignalStrength =
    convertCdmaSignalStrength(aSignalStrength.cdma);
  RefPtr<nsEvdoSignalStrength> evdoSignalStrength =
    convertEvdoSignalStrength(aSignalStrength.evdo);
  RefPtr<nsWcdmaSignalStrength> wcdmaSignalStrength =
    convertWcdmaSignalStrength_V1_2(aSignalStrength.wcdma);
  RefPtr<nsTdScdmaSignalStrength> tdscdmaSignalStrength =
    convertTdScdmaSignalStrength_V1_2(aSignalStrength.tdscdma);
  RefPtr<nsLteSignalStrength> lteSignalStrength =
    convertLteSignalStrength_V1_6(aSignalStrength.lte);
  RefPtr<nsNrSignalStrength> nrSignalStrength =
    convertNrSignalStrength_V1_6(&aSignalStrength.nr);

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

RefPtr<nsCellInfoLte>
nsRilResult::convertCellInfoLte_V1_6(const CellInfoLte_V1_6* aCellInfoLte)
{
  RefPtr<nsCellIdentityLte> cellIdentityLte =
    convertCellIdentityLte_V1_5(&aCellInfoLte->cellIdentityLte);
  RefPtr<nsLteSignalStrength> lteSignalStrength =
    convertLteSignalStrength_V1_6(aCellInfoLte->signalStrengthLte);
  RefPtr<nsCellConfigLte> cellConfigLte = nullptr;
  RefPtr<nsCellInfoLte> cellInfoLte =
    new nsCellInfoLte(cellIdentityLte, lteSignalStrength, cellConfigLte);

  return cellInfoLte;
}

RefPtr<nsCellInfoNr>
nsRilResult::convertCellInfoNr_V1_6(const CellInfoNr_V1_6* aCellInfoNr)
{
  RefPtr<nsCellIdentityNr> cellIdentityNr =
    convertCellIdentityNr_V1_5(&aCellInfoNr->cellIdentityNr);
  RefPtr<nsNrSignalStrength> nrSignalStrength =
    convertNrSignalStrength_V1_6(&aCellInfoNr->signalStrengthNr);
  RefPtr<nsCellInfoNr> cellInfoNr =
    new nsCellInfoNr(cellIdentityNr, nrSignalStrength);

  return cellInfoNr;
}

int32_t
nsRilResult::convertCellInfoTypeToRil_V1_6(uint8_t type)
{
  switch (type) {
    case (uint8_t)CellInfoType_V1_6::gsm:
      return nsICellInfoType::RADIO_CELL_INFO_TYPE_GSM;
    case (uint8_t)CellInfoType_V1_6::cdma:
      return nsICellInfoType::RADIO_CELL_INFO_TYPE_CDMA;
    case (uint8_t)CellInfoType_V1_6::lte:
      return nsICellInfoType::RADIO_CELL_INFO_TYPE_LTE;
    case (uint8_t)CellInfoType_V1_6::wcdma:
      return nsICellInfoType::RADIO_CELL_INFO_TYPE_WCDMA;
    case (uint8_t)CellInfoType_V1_6::tdscdma:
      return nsICellInfoType::RADIO_CELL_INFO_TYPE_TD_SCDMA;
    case (uint8_t)CellInfoType_V1_6::nr:
      return nsICellInfoType::RADIO_CELL_INFO_TYPE_NR;
    default:
      return nsICellInfoType::RADIO_CELL_INFO_TYPE_UNKNOW;
  }
}

RefPtr<nsRilCellInfo>
nsRilResult::convertRilCellInfo_V1_6(const CellInfo_V1_6* aCellInfo)
{
  int32_t cellInfoType = convertCellInfoTypeToRil_V1_6(
    (uint8_t)aCellInfo->ratSpecificInfo.getDiscriminator());
  bool registered = aCellInfo->registered;
  int32_t connectionStatus =
    convertConnectionStatus(aCellInfo->connectionStatus);
  int32_t timeStampType = -1;
  uint64_t timeStamp = 0;

  if (cellInfoType == nsICellInfoType::RADIO_CELL_INFO_TYPE_GSM) {
    RefPtr<nsCellInfoGsm> cellInfoGsm =
      convertCellInfoGsm_V1_5(&aCellInfo->ratSpecificInfo.gsm());
    RefPtr<nsRilCellInfo> rilCellInfoGsm = new nsRilCellInfo(cellInfoType,
                                                             registered,
                                                             timeStampType,
                                                             timeStamp,
                                                             cellInfoGsm,
                                                             connectionStatus);
    return rilCellInfoGsm;
  } else if (cellInfoType == nsICellInfoType::RADIO_CELL_INFO_TYPE_LTE) {
    RefPtr<nsCellInfoLte> cellInfoLte =
      convertCellInfoLte_V1_6(&aCellInfo->ratSpecificInfo.lte());
    RefPtr<nsRilCellInfo> rilCellInfoLte = new nsRilCellInfo(cellInfoType,
                                                             registered,
                                                             timeStampType,
                                                             timeStamp,
                                                             cellInfoLte,
                                                             connectionStatus);
    return rilCellInfoLte;
  } else if (cellInfoType == nsICellInfoType::RADIO_CELL_INFO_TYPE_WCDMA) {
    RefPtr<nsCellInfoWcdma> cellInfoWcdma =
      convertCellInfoWcdma_V1_5(&aCellInfo->ratSpecificInfo.wcdma());
    RefPtr<nsRilCellInfo> rilCellInfoWcdma =
      new nsRilCellInfo(cellInfoType,
                        registered,
                        timeStampType,
                        timeStamp,
                        cellInfoWcdma,
                        connectionStatus);
    return rilCellInfoWcdma;
  } else if (cellInfoType == nsICellInfoType::RADIO_CELL_INFO_TYPE_TD_SCDMA) {
    RefPtr<nsCellInfoTdScdma> cellInfoTdScdma =
      convertCellInfoTdScdma_V1_5(&aCellInfo->ratSpecificInfo.tdscdma());
    RefPtr<nsRilCellInfo> rilCellInfoTdScdma =
      new nsRilCellInfo(cellInfoType,
                        registered,
                        timeStampType,
                        timeStamp,
                        cellInfoTdScdma,
                        connectionStatus);
    return rilCellInfoTdScdma;
  } else if (cellInfoType == nsICellInfoType::RADIO_CELL_INFO_TYPE_CDMA) {
    RefPtr<nsCellInfoCdma> cellInfoCdma =
      convertCellInfoCdma_V1_2(&aCellInfo->ratSpecificInfo.cdma());
    RefPtr<nsRilCellInfo> rilCellInfoCdma = new nsRilCellInfo(cellInfoType,
                                                              registered,
                                                              timeStampType,
                                                              timeStamp,
                                                              cellInfoCdma,
                                                              connectionStatus);
    return rilCellInfoCdma;
  } else if (cellInfoType == nsICellInfoType::RADIO_CELL_INFO_TYPE_NR) {
    RefPtr<nsCellInfoNr> cellInfoNr =
      convertCellInfoNr_V1_6(&aCellInfo->ratSpecificInfo.nr());
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
#endif
NS_DEFINE_NAMED_CID(RILRESULT_CID);
