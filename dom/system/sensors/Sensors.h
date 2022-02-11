/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef B2G_SENSORS_H
#define B2G_SENSORS_H

#include "mozilla/Hal.h"
#include "mozilla/HalSensor.h"
#include "mozilla/StaticPtr.h"
#include "nsCOMArray.h"
#include "nsISensors.h"
#include "nsISensorsListener.h"

namespace b2g {

class Sensors final : public nsISensors,
                      public mozilla::hal::ISensorObserver {
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISENSORS

  // Get the singleton Sensors instance
  static already_AddRefed<Sensors> GetSingleton();

private:
  Sensors() = default;
  ~Sensors() = default;

  static mozilla::StaticRefPtr<Sensors> sSingleton;

  // Inherited method from ISensorObserver
  void Notify(const mozilla::hal::SensorData& aSensorData) override;

  // Registered listeners of various sensor types
  nsCOMArray<nsISensorsListener> mSensorListeners[mozilla::hal::NUM_SENSOR_TYPE];
};

} // namespace b2g

#endif // B2G_SENSORS_H
