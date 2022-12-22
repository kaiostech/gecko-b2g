/* (c) 2021 KAI OS TECHNOLOGIES (HONG KONG) LIMITED All rights reserved. This
 * file or any portion thereof may not be reproduced or used in any manner
 * whatsoever without the express written permission of KAI OS TECHNOLOGIES
 * (HONG KONG) LIMITED. KaiOS is the trademark of KAI OS TECHNOLOGIES (HONG
 * KONG) LIMITED or its affiliate company and may be registered in some
 * jurisdictions. All other trademarks are the property of their respective
 * owners.
 */

#ifndef b2g_GnssSvInfo_h
#define b2g_GnssSvInfo_h

#include "nsIGnssMonitor.h"

namespace b2g {

class GnssSvInfo final : public nsIGnssSvInfo {
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIGNSSSVINFO

  GnssSvInfo(int16_t aSvid, nsIGnssSvInfo::GnssConstellationType aConstellation,
             float aCN0Dbhz, float aElevationDegrees, float aAzimuthDegrees,
             float aCarrierFrequencyHz, uint8_t aSvFlag);

private:
  ~GnssSvInfo() = default;

  int16_t mSvid;
  nsIGnssSvInfo::GnssConstellationType mConstellation;
  float mCN0Dbhz;
  float mElevationDegrees;
  float mAzimuthDegrees;
  float mCarrierFrequencyHz;
  uint8_t mSvFlag;
};

} // namespace b2g

#endif // b2g_GnssSvInfo_h
