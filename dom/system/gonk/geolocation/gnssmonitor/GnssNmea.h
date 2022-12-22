/* (c) 2021 KAI OS TECHNOLOGIES (HONG KONG) LIMITED All rights reserved. This
 * file or any portion thereof may not be reproduced or used in any manner
 * whatsoever without the express written permission of KAI OS TECHNOLOGIES
 * (HONG KONG) LIMITED. KaiOS is the trademark of KAI OS TECHNOLOGIES (HONG
 * KONG) LIMITED or its affiliate company and may be registered in some
 * jurisdictions. All other trademarks are the property of their respective
 * owners.
 */

#ifndef b2g_GnssNmea_h
#define b2g_GnssNmea_h

#include "nsIGnssMonitor.h"
#include "nsString.h" // for nsCString

namespace b2g {

class GnssNmea final : public nsIGnssNmea {
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIGNSSNMEA

  GnssNmea(int64_t aGnssUtcTime, nsACString const &aMessage);

private:
  ~GnssNmea() = default;

  int64_t mGnssUtcTime;
  nsCString mMessage;
};

} // namespace b2g

#endif // b2g_GnssNmea_h
