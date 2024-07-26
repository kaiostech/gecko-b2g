/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Hal.h"

#include "GonkSensorsHal.h"

#undef HAL_LOG
#undef HAL_ERR
#define HAL_LOG(args...) \
  __android_log_print(ANDROID_LOG_INFO, "GonkSensors", ##args)
#define HAL_ERR(args...) \
  __android_log_print(ANDROID_LOG_ERROR, "GonkSensors", ##args)

inline double radToDeg(double aRad) { return aRad * (180.0 / M_PI); }

hal::SensorType getSensorType(int aSensorType) {
  switch (aSensorType) {
    case SENSOR_TYPE_ORIENTATION:
      return SENSOR_ORIENTATION;
    case SENSOR_TYPE_ACCELEROMETER:
      return SENSOR_ACCELERATION;
    case SENSOR_TYPE_PROXIMITY:
      return SENSOR_PROXIMITY;
    case SENSOR_TYPE_LIGHT:
      return SENSOR_LIGHT;
    case SENSOR_TYPE_GYROSCOPE:
      return SENSOR_GYROSCOPE;
    case SENSOR_TYPE_LINEAR_ACCELERATION:
      return SENSOR_LINEAR_ACCELERATION;
    case SENSOR_TYPE_ROTATION_VECTOR:
      return SENSOR_ROTATION_VECTOR;
    case SENSOR_TYPE_GAME_ROTATION_VECTOR:
      return SENSOR_GAME_ROTATION_VECTOR;
    case SENSOR_TYPE_PRESSURE:
      return SENSOR_PRESSURE;
    default:
      return SENSOR_UNKNOWN;
  }
}

namespace mozilla {
namespace hal_impl {

GonkSensorsHal* GonkSensorsHal::sInstance = nullptr;

class GonkSensorsHal::SensorDataNotifier : public Runnable {
 public:
  SensorDataNotifier(const SensorData aSensorData,
                     const SensorDataCallback aCallback)
      : mozilla::Runnable("GonkSensors::SensorDataNotifier"),
        mSensorData(aSensorData),
        mCallback(aCallback) {}

  NS_IMETHOD Run() override {
    if (mCallback) {
      mCallback(mSensorData);
    }
    return NS_OK;
  }

 private:
  SensorData mSensorData;
  SensorDataCallback mCallback;
};

SensorData GonkSensorsHal::CreateSensorData(const sensors_event_t aEvent) {
  AutoTArray<float, 4> values;

  hal::SensorType sensorType = getSensorType(aEvent.type);

  if (sensorType == SENSOR_UNKNOWN) {
    return SensorData(sensorType, aEvent.timestamp, values);
  }

  switch (sensorType) {
    case SENSOR_ORIENTATION:
      // Bug 938035, convert aosp hal orientation sensor data to meet the earth
      // coordinate frame defined by w3c spec
      values.AppendElement(-aEvent.orientation.azimuth + 360);
      values.AppendElement(-aEvent.orientation.pitch);
      values.AppendElement(-aEvent.orientation.roll);
      break;
    case SENSOR_ACCELERATION:
    case SENSOR_LINEAR_ACCELERATION:
      values.AppendElement(aEvent.acceleration.x);
      values.AppendElement(aEvent.acceleration.y);
      values.AppendElement(aEvent.acceleration.z);
      break;
    case SENSOR_PROXIMITY:
      values.AppendElement(aEvent.distance);
      values.AppendElement(0);
      values.AppendElement(mSensorList[sensorType].maxRange);
      break;
    case SENSOR_GYROSCOPE:
      // convert aosp hal gyroscope sensor data from radians per second into
      // degrees per second that defined by w3c spec
      values.AppendElement(radToDeg(aEvent.gyro.v[0]));
      values.AppendElement(radToDeg(aEvent.gyro.v[1]));
      values.AppendElement(radToDeg(aEvent.gyro.v[2]));
      break;
    case SENSOR_LIGHT:
      values.AppendElement(aEvent.light);
      break;
    case SENSOR_ROTATION_VECTOR:
    case SENSOR_GAME_ROTATION_VECTOR:
      values.AppendElement(aEvent.data[0]);
      values.AppendElement(aEvent.data[1]);
      values.AppendElement(aEvent.data[2]);
      values.AppendElement(aEvent.data[3]);
      break;
    case SENSOR_PRESSURE:
      values.AppendElement(aEvent.pressure);
      break;
    case SENSOR_UNKNOWN:
    default:
      HAL_ERR("invalid sensor type");
      break;
  }

  return SensorData(sensorType, aEvent.timestamp, values);
}

void GonkSensorsHal::StartPollingThread() {
  if (mPollThread == nullptr) {
    nsresult rv = NS_NewNamedThread("SensorsPoll", getter_AddRefs(mPollThread));
    if (NS_FAILED(rv)) {
      HAL_ERR("sensors poll thread created failed");
      mPollThread = nullptr;
      return;
    }
  }

  mPollThread->Dispatch(
      NS_NewRunnableFunction(
          "Polling",
          [this]() {
            do {
              ssize_t eventsRead = 0;
              size_t numEventMax = mSensorEventBuffer.size();

              // reading from fmq is preferred than polling hal as it is based
              // on shared memory
              if (mHalWrapper->supportsMessageQueues()) {
                eventsRead = mHalWrapper->pollFmq(mSensorEventBuffer.data(), numEventMax);
              } else if (mHalWrapper->supportsPolling()) {
                // TODO: to implement the polling for hidl-based wrapper
                eventsRead = -1;
              } else {
                // can't reach here, it must support either polling or fmq
                HAL_ERR("sensors hal must support either polling or fmq");
                break;
              }

              // create sensor data and dispatch to main thread
              if (eventsRead > 0) {
                for (ssize_t i = 0; i < eventsRead; i++) {
                  SensorData sensorData = CreateSensorData(mSensorEventBuffer[i]);

                  if (sensorData.sensor() == SENSOR_UNKNOWN) {
                    continue;
                  }

                  // TODO: Bug 123480 to notify the count of wakeup events to
                  // wakelock queue


                  NS_DispatchToMainThread(
                      new SensorDataNotifier(sensorData, mSensorDataCallback));
                }
              }
              // stop polling sensors data if it is reconnecting
            } while (!mHalWrapper->mReconnecting.load());

            if (mHalWrapper->mReconnecting.load()) {
              Reconnect();
            }
          }),
      NS_DISPATCH_NORMAL);
}

void GonkSensorsHal::Init() {
  // connect sensors hal for aidl hal
  if (!ConnectAidlHalService()) {
    HAL_ERR("connect sensors aidl hal failed");
    mHalWrapper = nullptr;
    return;
  }

  // TODO: to implement connecting for hidl hal

  // initialize available sensors list
  if (!InitSensorsList()) {
    HAL_ERR("initialize sensors list failed");
    mHalWrapper = nullptr;
    return;
  }

  // start a polling thread reading sensors events
  StartPollingThread();

  HAL_LOG("sensors init completed");
}

bool GonkSensorsHal::ConnectAidlHalService() {
  HAL_LOG("sensors connect aidl hal wrapper");

  mHalWrapper = std::make_unique<android::AidlSensorHalWrapper>();
  if (!mHalWrapper->connect(this)) {
    return false;
  }

  return true;
}

bool GonkSensorsHal::InitSensorsList() {
  if (mHalWrapper == nullptr) {
    HAL_ERR("init sensors list failed, no hal wrapper");
    return false;
  }

  // obtain sensors list from hal wrapper
  std::vector<sensor_t> list = mHalWrapper->getSensorsList();

  // setup the available sensors list and filter out unnecessary ones
  for (sensor_t &sensor : list) {
    hal::SensorType sensorType = getSensorType(sensor.type);

    int32_t reportingMode = sensor.flags & SENSOR_FLAG_MASK_REPORTING_MODE;
    bool isContinuous = reportingMode == SENSOR_FLAG_CONTINUOUS_MODE;
    bool isOnchange = reportingMode == SENSOR_FLAG_ON_CHANGE_MODE;
    bool canWakeUp = sensor.flags & SENSOR_FLAG_WAKE_UP;

    // check sensor reporting mode and wake-up capability
    bool isValid = false;
    switch (sensorType) {
      case SENSOR_PROXIMITY:
        isValid = (isOnchange && canWakeUp);
        break;
      case SENSOR_LIGHT:
        isValid = (isOnchange && !canWakeUp);
        break;
      case SENSOR_ORIENTATION:
      case SENSOR_ACCELERATION:
      case SENSOR_GYROSCOPE:
      case SENSOR_LINEAR_ACCELERATION:
      case SENSOR_ROTATION_VECTOR:
      case SENSOR_GAME_ROTATION_VECTOR:
      case SENSOR_PRESSURE:
        isValid = (isContinuous && !canWakeUp);
        break;
      default:
        break;
    }
    if (isValid) {
      mSensorList[sensorType] = sensor;
      HAL_LOG("a valid sensor type=%d handle=%d is initialized", sensorType, sensor.handle);
    }
  }

  return true;
}

bool GonkSensorsHal::RegisterSensorDataCallback(
    const SensorDataCallback aCallback) {
  mSensorDataCallback = aCallback;
  return true;
};

bool GonkSensorsHal::ActivateSensor(const hal::SensorType aSensorType) {
  MutexAutoLock lock(mLock);

  if (mHalWrapper == nullptr) {
    HAL_ERR("activate sensor failed, no hal wrapper");
    return false;
  }

  bool hasSensor = (bool)mSensorList[aSensorType].type;

  // check if specified sensor is supported
  if (!hasSensor) {
    HAL_LOG("device unsupported sensor aSensorType=%d", aSensorType);
    return false;
  }
  HAL_LOG("activate aSensorType=%d", aSensorType);

  const int handle = mSensorList[aSensorType].handle;

  int64_t samplingPeriodNs;
  switch (aSensorType) {
    // no sampling period for on-change sensors
    case SENSOR_PROXIMITY:
    case SENSOR_LIGHT:
      samplingPeriodNs = 0;
      break;
    // specific sampling period for pressure sensor
    case SENSOR_PRESSURE:
      samplingPeriodNs = kPressureSamplingPeriodNs;
      break;
    // default sampling period for most continuous sensors
    case SENSOR_ORIENTATION:
    case SENSOR_ACCELERATION:
    case SENSOR_LINEAR_ACCELERATION:
    case SENSOR_GYROSCOPE:
    case SENSOR_ROTATION_VECTOR:
    case SENSOR_GAME_ROTATION_VECTOR:
    default:
      samplingPeriodNs = kDefaultSamplingPeriodNs;
      break;
  }

  int32_t minDelayNs = mSensorList[aSensorType].minDelay * 1000;
  if (samplingPeriodNs < minDelayNs) {
    samplingPeriodNs = minDelayNs;
  }

  // config sampling period and reporting latency to specified sensor
  auto ret = mHalWrapper->batch(handle, samplingPeriodNs, kReportLatencyNs);
  if (ret != android::OK) {
    HAL_ERR("sensors batch failed aSensorType=%d", aSensorType);
    return false;
  }

  // activate specified sensor
  ret = mHalWrapper->activate(handle, true);
  if (ret != android::OK) {
    HAL_ERR("sensors activate failed aSensorType=%d", aSensorType);
    return false;
  }

  return true;
}

bool GonkSensorsHal::DeactivateSensor(const hal::SensorType aSensorType) {
  MutexAutoLock lock(mLock);

  if (mHalWrapper == nullptr) {
    HAL_ERR("deactivate sensor failed, no hal wrapper");
    return false;
  }

  bool hasSensor = (bool)mSensorList[aSensorType].type;

  // check if specified sensor is supported
  if (!hasSensor) {
    HAL_LOG("device unsupported sensor aSensorType=%d", aSensorType);
    return false;
  }
  HAL_LOG("deactivate aSensorType=%d", aSensorType);

  const int handle = mSensorList[aSensorType].handle;

  // deactivate specified sensor
  auto ret = mHalWrapper->activate(handle, false);
  if (ret != android::OK) {
    HAL_ERR("sensors deactivate failed aSensorType=%d", aSensorType);
    return false;
  }

  return true;
}

void GonkSensorsHal::GetSensorVendor(const hal::SensorType aSensorType,
                                     nsACString& aRetval) {
  sensor_t& sensor = mSensorList[aSensorType];
  aRetval.AssignASCII(sensor.vendor);
}

void GonkSensorsHal::GetSensorName(const hal::SensorType aSensorType,
                                   nsACString& aRetval) {
  sensor_t& sensor = mSensorList[aSensorType];
  aRetval.AssignASCII(sensor.name);
}

void GonkSensorsHal::Reconnect() {
  MutexAutoLock lock(mLock);

  HAL_LOG("reconnecting sensors hal");
  Init();

  // TODO: to recover sensors active/inactive state, tracked by Bug 124978
}

// TODO: to support dynamic sensors connection if there is requirement
void GonkSensorsHal::onDynamicSensorsConnected(const std::vector<sensor_t>& dynamicSensorsAdded) {
  HAL_LOG("%s", __func__);
}

void GonkSensorsHal::onDynamicSensorsDisconnected(const std::vector<int32_t> &dynamicSensorHandlesRemoved) {
  HAL_LOG("%s", __func__);
}

}  // namespace hal_impl
}  // namespace mozilla
