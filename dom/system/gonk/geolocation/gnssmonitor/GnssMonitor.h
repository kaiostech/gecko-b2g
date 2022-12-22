/* (c) 2021 KAI OS TECHNOLOGIES (HONG KONG) LIMITED All rights reserved. This
 * file or any portion thereof may not be reproduced or used in any manner
 * whatsoever without the express written permission of KAI OS TECHNOLOGIES
 * (HONG KONG) LIMITED. KaiOS is the trademark of KAI OS TECHNOLOGIES (HONG
 * KONG) LIMITED or its affiliate company and may be registered in some
 * jurisdictions. All other trademarks are the property of their respective
 * owners.
 */

#ifndef b2g_GnssMonitor_h
#define b2g_GnssMonitor_h

#include "mozilla/StaticPtr.h"
#include "nsCOMArray.h"
#include "nsIGnssMonitor.h"

namespace b2g {

class GnssMonitor final : public nsIGnssMonitor {
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIGNSSMONITOR

  // Get the singleton GnssMonitor instance
  static already_AddRefed<GnssMonitor> GetSingleton();

private:
  GnssMonitor() = default;
  ~GnssMonitor() = default;

  static mozilla::StaticRefPtr<GnssMonitor> sSingleton;

  nsCOMArray<nsIGnssListener> mListeners;
  nsIGnssMonitor::GnssStatusValue mStatus;
  nsTArray<RefPtr<nsIGnssSvInfo>> mSvList;
  RefPtr<nsIGnssNmea> mNmeaData;
};

} // namespace b2g

#endif // b2g_GnssMonitor_h
