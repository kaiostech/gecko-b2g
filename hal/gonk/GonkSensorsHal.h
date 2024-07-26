/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef HAL_GONK_GONKSENSORSHAL_H_
#define HAL_GONK_GONKSENSORSHAL_H_

#include "mozilla/HalSensor.h"
#include "mozilla/Mutex.h"
#include "nsThreadUtils.h"

#include "aidl/android/hardware/sensors/ISensors.h"
#include "gonk/AidlSensorHalWrapper.h"

using namespace mozilla::hal;

namespace mozilla {
namespace hal_impl {

typedef void (*SensorDataCallback)(const SensorData& aSensorData);

class GonkSensorsHal :
 public android::ISensorHalWrapper::SensorDeviceCallback {
 public:
  static GonkSensorsHal* GetInstance() {
    if (sInstance == nullptr) {
      sInstance = new GonkSensorsHal();
    }
    return sInstance;
  };

  bool RegisterSensorDataCallback(const SensorDataCallback aCallback);
  bool ActivateSensor(const hal::SensorType aSensorType);
  bool DeactivateSensor(const hal::SensorType aSensorType);
  void GetSensorVendor(const hal::SensorType aSensorType, nsACString& aRetval);
  void GetSensorName(const hal::SensorType aSensorType, nsACString& aRetval);

 private:
  class SensorDataNotifier;

  static GonkSensorsHal* sInstance;

  GonkSensorsHal() :
        mHalWrapper(nullptr),
        mPollThread(nullptr),
        mLock("Sensors"),
        mSensorDataCallback(nullptr) {
    Init();
  };
  ~GonkSensorsHal(){};

  void Init();
  bool ConnectAidlHalService();
  bool InitSensorsList();
  void Reconnect();
  void StartPollingThread();
  SensorData CreateSensorData(const sensors_event_t aEvent);

  void onDynamicSensorsConnected(const std::vector<sensor_t>& dynamicSensorsAdded);
  void onDynamicSensorsDisconnected(const std::vector<int32_t> &dynamicSensorHandlesRemoved);

  std::unique_ptr<android::ISensorHalWrapper> mHalWrapper;
  sensor_t mSensorList[NUM_SENSOR_TYPE];
  nsCOMPtr<nsIThread> mPollThread;
  Mutex mLock;
  SensorDataCallback mSensorDataCallback;
  std::array<sensors_event_t, MAX_EVENT_BUFFER_SIZE> mSensorEventBuffer;

  const int64_t kDefaultSamplingPeriodNs = 200000000;
  const int64_t kPressureSamplingPeriodNs = 100000000;
  const int64_t kReportLatencyNs = 0;
};

}  // namespace hal_impl
}  // namespace mozilla

#endif  // HAL_GONK_GONKSENSORSHAL_H_
