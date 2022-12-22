/* (c) 2021 KAI OS TECHNOLOGIES (HONG KONG) LIMITED All rights reserved. This
 * file or any portion thereof may not be reproduced or used in any manner
 * whatsoever without the express written permission of KAI OS TECHNOLOGIES
 * (HONG KONG) LIMITED. KaiOS is the trademark of KAI OS TECHNOLOGIES (HONG
 * KONG) LIMITED or its affiliate company and may be registered in some
 * jurisdictions. All other trademarks are the property of their respective
 * owners.
 */

#include "GnssNmea.h"
#include "nsISupportsImpl.h"

namespace b2g {

NS_IMPL_ISUPPORTS(GnssNmea, nsIGnssNmea)

GnssNmea::GnssNmea(int64_t aGnssUtcTime, nsACString const &aMessage)
    : mGnssUtcTime(aGnssUtcTime), mMessage(aMessage) {}

NS_IMETHODIMP GnssNmea::GetGnssUtcTime(int64_t *aGnssUtcTime) {
  *aGnssUtcTime = mGnssUtcTime;
  return NS_OK;
}

NS_IMETHODIMP GnssNmea::GetMessage(nsACString &aMessage) {
  aMessage = mMessage;
  return NS_OK;
}

} // namespace b2g
