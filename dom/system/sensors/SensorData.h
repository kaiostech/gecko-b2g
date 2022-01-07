/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef B2G_SENSOR_DATA_H
#define B2G_SENSOR_DATA_H

#include "nsISensorData.h"

namespace b2g {

class SensorData final : public nsISensorData {
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISENSORDATA

  SensorData(nsISensors::SensorType type, double x, double y, double z, double w);

private:
  ~SensorData() = default;

  nsISensors::SensorType mType;
  double mX;
  double mY;
  double mZ;
  double mW;
};

} // namespace b2g

#endif // B2G_SENSOR_DATA_H
