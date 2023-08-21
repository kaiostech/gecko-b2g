/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Sensors.h"

#include "mozilla/ClearOnShutdown.h"
#include "nsISupportsImpl.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "SensorData.h"
#ifdef MOZ_WIDGET_GONK
#  include "gonk/GonkSensorsHal.h"
#endif

using namespace mozilla;

namespace b2g {

StaticRefPtr<Sensors> Sensors::sSingleton;

NS_IMPL_ISUPPORTS(Sensors, nsISensors)

already_AddRefed<Sensors> Sensors::GetSingleton() {
  if (!XRE_IsParentProcess()) {
    return nullptr;
  }

  MOZ_ASSERT(NS_IsMainThread());

  if (!sSingleton) {
    sSingleton = new Sensors();
    ClearOnShutdown(&sSingleton);
  }

  RefPtr<Sensors> singleton = sSingleton.get();
  return singleton.forget();
}

NS_IMETHODIMP Sensors::RegisterListener(nsISensorsListener* aListener,
                                        nsISensors::SensorType aSensorType) {
  MOZ_ASSERT(NS_IsMainThread());
  hal::SensorType sensorType = static_cast<hal::SensorType>(aSensorType);
  if (sensorType >= hal::NUM_SENSOR_TYPE) {
    return NS_ERROR_UNEXPECTED;
  }

  nsCOMArray<nsISensorsListener>& listeners = mSensorListeners[sensorType];

  if (!listeners.Contains(aListener)) {
    listeners.AppendObject(aListener);
  }

  if (listeners.Length() == 1) {
    hal::RegisterSensorObserver(sensorType, this);
  }

  return NS_OK;
}

NS_IMETHODIMP Sensors::UnregisterListener(nsISensorsListener* aListener,
                                          nsISensors::SensorType aSensorType) {
  MOZ_ASSERT(NS_IsMainThread());
  hal::SensorType sensorType = static_cast<hal::SensorType>(aSensorType);
  if (sensorType >= hal::NUM_SENSOR_TYPE) {
    return NS_ERROR_UNEXPECTED;
  }

  nsCOMArray<nsISensorsListener>& listeners = mSensorListeners[sensorType];

  if (listeners.Contains(aListener)) {
    listeners.RemoveObject(aListener);
  }

  if (listeners.Length() == 0) {
    hal::UnregisterSensorObserver(sensorType, this);
  }

  return NS_OK;
}

NS_IMETHODIMP Sensors::GetVendor(nsISensors::SensorType aSensorType,
                                 nsACString& aRetval) {
  MOZ_ASSERT(NS_IsMainThread());
  hal::SensorType sensorType = static_cast<hal::SensorType>(aSensorType);
  if (sensorType >= hal::NUM_SENSOR_TYPE) {
    return NS_ERROR_UNEXPECTED;
  }

#ifdef MOZ_WIDGET_GONK
  hal_impl::GonkSensorsHal::GetInstance()->GetSensorVendor(sensorType, aRetval);
#endif

  return NS_OK;
}

NS_IMETHODIMP Sensors::GetName(nsISensors::SensorType aSensorType,
                               nsACString& aRetval) {
  MOZ_ASSERT(NS_IsMainThread());
  hal::SensorType sensorType = static_cast<hal::SensorType>(aSensorType);
  if (sensorType >= hal::NUM_SENSOR_TYPE) {
    return NS_ERROR_UNEXPECTED;
  }

#ifdef MOZ_WIDGET_GONK
  hal_impl::GonkSensorsHal::GetInstance()->GetSensorName(sensorType, aRetval);
#endif

  return NS_OK;
}

void Sensors::Notify(const hal::SensorData& aSensorData) {
  MOZ_ASSERT(NS_IsMainThread());

  hal::SensorType sensorType = aSensorData.sensor();
  const nsTArray<float>& values = aSensorData.values();
  size_t len = values.Length();
  double x = len > 0 ? values[0] : 0.0;
  double y = len > 1 ? values[1] : 0.0;
  double z = len > 2 ? values[2] : 0.0;
  double w = len > 3 ? values[3] : 0.0;

  nsCOMArray<nsISensorsListener>& listeners = mSensorListeners[sensorType];

  RefPtr<nsISensorData> data = new SensorData(
      static_cast<nsISensors::SensorType>(sensorType), x, y, z, w);

  for (nsISensorsListener* listener : listeners) {
    listener->OnSensorDataUpdate(data);
  }
}

}  // namespace b2g
