/* (c) 2021 KAI OS TECHNOLOGIES (HONG KONG) LIMITED All rights reserved. This
 * file or any portion thereof may not be reproduced or used in any manner
 * whatsoever without the express written permission of KAI OS TECHNOLOGIES
 * (HONG KONG) LIMITED. KaiOS is the trademark of KAI OS TECHNOLOGIES (HONG
 * KONG) LIMITED or its affiliate company and may be registered in some
 * jurisdictions. All other trademarks are the property of their respective
 * owners.
 */

#include "GnssSvInfo.h"
#include "nsISupportsImpl.h"

namespace b2g {

NS_IMPL_ISUPPORTS(GnssSvInfo, nsIGnssSvInfo)

GnssSvInfo::GnssSvInfo(int16_t aSvid,
                       nsIGnssSvInfo::GnssConstellationType aConstellation,
                       float aCN0Dbhz, float aElevationDegrees,
                       float aAzimuthDegrees, float aCarrierFrequencyHz,
                       uint8_t aSvFlag)
    : mSvid(aSvid), mConstellation(aConstellation), mCN0Dbhz(aCN0Dbhz),
      mElevationDegrees(aElevationDegrees), mAzimuthDegrees(aAzimuthDegrees),
      mCarrierFrequencyHz(aCarrierFrequencyHz), mSvFlag(aSvFlag) {}

NS_IMETHODIMP GnssSvInfo::GetSvid(int16_t *aSvid) {
  *aSvid = mSvid;
  return NS_OK;
}

NS_IMETHODIMP GnssSvInfo::GetConstellation(
    nsIGnssSvInfo::GnssConstellationType *aConstellation) {
  *aConstellation = mConstellation;
  return NS_OK;
}

NS_IMETHODIMP GnssSvInfo::GetCN0Dbhz(float *aCN0Dbhz) {
  *aCN0Dbhz = mCN0Dbhz;
  return NS_OK;
}

NS_IMETHODIMP GnssSvInfo::GetElevationDegrees(float *aElevationDegrees) {
  *aElevationDegrees = mElevationDegrees;
  return NS_OK;
}

NS_IMETHODIMP GnssSvInfo::GetAzimuthDegrees(float *aAzimuthDegrees) {
  *aAzimuthDegrees = mAzimuthDegrees;
  return NS_OK;
}

NS_IMETHODIMP GnssSvInfo::GetCarrierFrequencyHz(float *aCarrierFrequencyHz) {
  *aCarrierFrequencyHz = mCarrierFrequencyHz;
  return NS_OK;
}

NS_IMETHODIMP GnssSvInfo::GetSvFlag(uint8_t *aSvFlag) {
  *aSvFlag = mSvFlag;
  return NS_OK;
}

} // namespace b2g
