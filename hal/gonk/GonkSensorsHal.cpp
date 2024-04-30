/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Hal.h"

#include "GonkSensorsHal.h"

#define HAL_LOG(args...) \
  __android_log_print(ANDROID_LOG_INFO, "GonkSensors", ##args)
#define HAL_ERR(args...) \
  __android_log_print(ANDROID_LOG_ERROR, "GonkSensors", ##args)

inline double radToDeg(double aRad) { return aRad * (180.0 / M_PI); }

template <typename EnumType>
constexpr typename std::underlying_type<EnumType>::type asBaseType(
    EnumType aValue) {
  return static_cast<typename std::underlying_type<EnumType>::type>(aValue);
}

enum EventQueueFlagBitsInternal : uint32_t {
  INTERNAL_WAKE = 1 << 16,
};

#ifdef HAL_HIDL_V21
hal::SensorType getSensorType(V2_1::SensorType aHidlSensorType) {
  using V2_1::SensorType;
#else
hal::SensorType getSensorType(V1_0::SensorType aHidlSensorType) {
  using V1_0::SensorType;
#endif
  switch (aHidlSensorType) {
    case SensorType::ORIENTATION:
      return SENSOR_ORIENTATION;
    case SensorType::ACCELEROMETER:
      return SENSOR_ACCELERATION;
    case SensorType::PROXIMITY:
      return SENSOR_PROXIMITY;
    case SensorType::LIGHT:
      return SENSOR_LIGHT;
    case SensorType::GYROSCOPE:
      return SENSOR_GYROSCOPE;
    case SensorType::LINEAR_ACCELERATION:
      return SENSOR_LINEAR_ACCELERATION;
    case SensorType::ROTATION_VECTOR:
      return SENSOR_ROTATION_VECTOR;
    case SensorType::GAME_ROTATION_VECTOR:
      return SENSOR_GAME_ROTATION_VECTOR;
    case SensorType::PRESSURE:
      return SENSOR_PRESSURE;
    default:
      return SENSOR_UNKNOWN;
  }
}

namespace mozilla {
namespace hal_impl {

struct SensorsCallback : public ISensorsCallback {
  // TODO: to support dynamic sensors connection if there is requirement
  Return<void> onDynamicSensorsConnected(
      const hidl_vec<V1_0::SensorInfo>& aDynamicSensorsAdded) override {
    return Void();
  }

  Return<void> onDynamicSensorsDisconnected(
      const hidl_vec<int32_t>& aDynamicSensorHandlesRemoved) override {
    return Void();
  }

#ifdef HAL_HIDL_V21
  Return<void> onDynamicSensorsConnected_2_1(
      const hidl_vec<V2_1::SensorInfo>& dynamicSensorsAdded) override {
    return Void();
  }
#endif
};

void SensorsHalDeathRecipient::serviceDied(
    uint64_t cookie,
    const android::wp<android::hidl::base::V1_0::IBase>& service) {
  HAL_ERR("sensors hidl server died");

  GonkSensorsHal::GetInstance()->PrepareForReconnect();
}

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

SensorData GonkSensorsHal::CreateSensorData(const Event aEvent) {
  AutoTArray<float, 4> values;

  hal::SensorType sensorType = getSensorType(aEvent.sensorType);

  if (sensorType == SENSOR_UNKNOWN) {
    return SensorData(sensorType, aEvent.timestamp, values);
  }

  switch (sensorType) {
    case SENSOR_ORIENTATION:
      // Bug 938035, convert hidl orientation sensor data to meet the earth
      // coordinate frame defined by w3c spec
      values.AppendElement(-aEvent.u.vec3.x + 360);
      values.AppendElement(-aEvent.u.vec3.y);
      values.AppendElement(-aEvent.u.vec3.z);
      break;
    case SENSOR_ACCELERATION:
    case SENSOR_LINEAR_ACCELERATION:
      values.AppendElement(aEvent.u.vec3.x);
      values.AppendElement(aEvent.u.vec3.y);
      values.AppendElement(aEvent.u.vec3.z);
      break;
    case SENSOR_PROXIMITY:
      values.AppendElement(aEvent.u.scalar);
      values.AppendElement(0);
      values.AppendElement(mSensorInfoList[sensorType].maxRange);
      break;
    case SENSOR_GYROSCOPE:
      // convert hidl gyroscope sensor data from radians per second into
      // degrees per second that defined by w3c spec
      values.AppendElement(radToDeg(aEvent.u.vec3.x));
      values.AppendElement(radToDeg(aEvent.u.vec3.y));
      values.AppendElement(radToDeg(aEvent.u.vec3.z));
      break;
    case SENSOR_LIGHT:
      values.AppendElement(aEvent.u.scalar);
      break;
    case SENSOR_ROTATION_VECTOR:
    case SENSOR_GAME_ROTATION_VECTOR:
      values.AppendElement(aEvent.u.vec4.x);
      values.AppendElement(aEvent.u.vec4.y);
      values.AppendElement(aEvent.u.vec4.z);
      values.AppendElement(aEvent.u.vec4.w);
      break;
    case SENSOR_PRESSURE:
      values.AppendElement(aEvent.u.scalar);
      break;
    case SENSOR_UNKNOWN:
    default:
      NS_ERROR("invalid sensor type");
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
              size_t eventsRead = 0;

              // reading from fmq is preferred than polling hal as it is based
              // on shared memory
              if (mSensors->supportsMessageQueues()) {
                eventsRead = PollFmq();
              } else if (mSensors->supportsPolling()) {
                eventsRead = PollHal();
              } else {
                // can't reach here, it must support polling or fmq
                HAL_ERR("sensors hal must support polling or fmq");
                break;
              }

              // create sensor data and dispatch to main thread
              for (size_t i = 0; i < eventsRead; i++) {
                SensorData sensorData = CreateSensorData(mEventBuffer[i]);

                if (sensorData.sensor() == SENSOR_UNKNOWN) {
                  continue;
                }

                // TODO: Bug 123480 to notify the count of wakeup events to
                // wakelock queue

                NS_DispatchToMainThread(
                    new SensorDataNotifier(sensorData, mSensorDataCallback));
              }
              // stop polling sensors data if it is reconnecting
            } while (!mToReconnect);

            if (mToReconnect) {
              Reconnect();
            }
          }),
      NS_DISPATCH_NORMAL);
}

size_t GonkSensorsHal::PollHal() {
  hidl_vec<Event> events;
  size_t eventsRead = 0;

  auto ret = mSensors->poll(
      mEventBuffer.size(),
      [&events](auto result, const auto& data, const auto&) {
        if (result == V1_0::Result::OK) {
#ifdef HAL_HIDL_V21
          events = reinterpret_cast<const hidl_vec<V2_1::Event>&>(data);
#else
          events = data;
#endif
        } else {
          HAL_ERR("polling sensors event result failed");
        }
      });

  if (ret.isOk()) {
    eventsRead = events.size();
    for (size_t i = 0; i < eventsRead; i++) {
      mEventBuffer[i] = events[i];
    }
  } else {
    HAL_ERR("polling sensors event failed");
  }

  return eventsRead;
}

size_t GonkSensorsHal::PollFmq() {
  size_t eventsRead = 0;

  size_t availableEvents = mSensors->getEventQueue()->availableToRead();
  if (availableEvents == 0) {
    uint32_t eventFlagState = 0;
    mEventQueueFlag->wait(asBaseType(EventQueueFlagBits::READ_AND_PROCESS) |
                              asBaseType(INTERNAL_WAKE),
                          &eventFlagState);
    availableEvents = mSensors->getEventQueue()->availableToRead();

    if ((eventFlagState & asBaseType(INTERNAL_WAKE)) && mToReconnect) {
      HAL_LOG("sensors poll thread is awaken up by internal wake");
      return 0;
    }
  }

  size_t eventsToRead = std::min(availableEvents, mEventBuffer.size());
  if (eventsToRead > 0) {
    if (mSensors->getEventQueue()->read(mEventBuffer.data(), eventsToRead)) {
      mEventQueueFlag->wake(asBaseType(EventQueueFlagBits::EVENTS_READ));

      eventsRead = eventsToRead;
    } else {
      HAL_ERR("reading sensors event fmq failed");
    }
  }

  return eventsRead;
}

void GonkSensorsHal::Init() {
  // initialize sensors hidl service
  if (!InitHidlService()) {
    HAL_ERR("initialize sensors hidl service failed");
    mSensors = nullptr;
    return;
  }

  // initialize available sensors list
  if (!InitSensorsList()) {
    HAL_ERR("initialize sensors list failed");
    mSensors = nullptr;
    return;
  }

  // start a polling thread reading sensors events
  StartPollingThread();

  HAL_LOG("sensors init completed");
}

bool GonkSensorsHal::InitHidlService() {
  // TODO: consider to move out initialization stuff from main thread
#ifdef HAL_HIDL_V21
  android::sp<V2_1::ISensors> serviceV2_1 = V2_1::ISensors::getService();
  if (serviceV2_1) {
    HAL_LOG("sensors v2.1 hidl service is detected");
    return InitHidlServiceV2_1(serviceV2_1);
  }
#endif

  android::sp<V2_0::ISensors> serviceV2_0 = V2_0::ISensors::getService();
  if (serviceV2_0) {
    HAL_LOG("sensors v2.0 hidl service is detected");
    return InitHidlServiceV2_0(serviceV2_0);
  }

  android::sp<V1_0::ISensors> serviceV1_0 = V1_0::ISensors::getService();
  if (serviceV1_0) {
    HAL_LOG("sensors v1.0 hidl service is detected");
    return InitHidlServiceV1_0(serviceV1_0);
  }

  HAL_ERR("no sensors hidl service is detected");
  return false;
}

bool GonkSensorsHal::InitHidlServiceV1_0(
    android::sp<V1_0::ISensors> aServiceV1_0) {
  size_t retry = 5;
  do {
    mSensors = new SensorsWrapperV1_0(aServiceV1_0);

    // poke hidl service to check if it is alive, the hidl service will kill and
    // restart itself if has lingering connection
    auto ret = mSensors->poll(0, [](auto, const auto&, const auto&) {});
    if (ret.isOk()) {
      mSensorsHalDeathRecipient = new SensorsHalDeathRecipient();
      aServiceV1_0->linkToDeath(mSensorsHalDeathRecipient, 0);
      return true;
    }

    if (retry > 0) {
      // sleep and wait for hidl service restarting
      HAL_ERR("sleep and wait for sensors hidl service restarting");
      usleep(50000);
      aServiceV1_0 = V1_0::ISensors::getService();
    }
  } while (retry-- > 0);

  HAL_ERR("sensors v1.0 hidl service initialize failed");

  return false;
}

bool GonkSensorsHal::InitHidlServiceV2_0(
    android::sp<V2_0::ISensors> aServiceV2_0) {
  mSensors = new SensorsWrapperV2_0(aServiceV2_0);

  mWakeLockQueue = std::make_unique<WakeLockQueue>(MAX_EVENT_BUFFER_SIZE, true);

  EventFlag::deleteEventFlag(&mEventQueueFlag);
  EventFlag::createEventFlag(mSensors->getEventQueue()->getEventFlagWord(),
                             &mEventQueueFlag);

  if (mSensors->getEventQueue() == nullptr || mWakeLockQueue == nullptr ||
      mEventQueueFlag == nullptr) {
    HAL_ERR("create sensors event queue failed");
    return false;
  }

  auto ret =
      mSensors->initialize(*mWakeLockQueue->getDesc(), new SensorsCallback());
  if (!ret.isOk() || ret != V1_0::Result::OK) {
    HAL_ERR("sensors v2.0 hidl service initialize failed");
    return false;
  }

  mSensorsHalDeathRecipient = new SensorsHalDeathRecipient();
  aServiceV2_0->linkToDeath(mSensorsHalDeathRecipient, 0);
  return true;
}

#ifdef HAL_HIDL_V21
bool GonkSensorsHal::InitHidlServiceV2_1(
    android::sp<V2_1::ISensors> aServiceV2_1) {
  mSensors = new SensorsWrapperV2_1(aServiceV2_1);

  mWakeLockQueue = std::make_unique<WakeLockQueue>(MAX_EVENT_BUFFER_SIZE, true);

  EventFlag::deleteEventFlag(&mEventQueueFlag);
  EventFlag::createEventFlag(mSensors->getEventQueue()->getEventFlagWord(),
                             &mEventQueueFlag);

  if (mSensors->getEventQueue() == nullptr || mWakeLockQueue == nullptr ||
      mEventQueueFlag == nullptr) {
    HAL_ERR("create sensors event queue failed");
    return false;
  }

  auto ret =
      mSensors->initialize(*mWakeLockQueue->getDesc(), new SensorsCallback());
  if (!ret.isOk() || ret != V1_0::Result::OK) {
    HAL_ERR("sensors v2.1 hidl service initialize failed");
    return false;
  }

  mSensorsHalDeathRecipient = new SensorsHalDeathRecipient();
  aServiceV2_1->linkToDeath(mSensorsHalDeathRecipient, 0);
  return true;
}
#endif

bool GonkSensorsHal::InitSensorsList() {
  if (mSensors == nullptr) {
    return false;
  }

  // obtain hidl sensors list
  hidl_vec<SensorInfo> list;
  auto ret =
      mSensors->getSensorsList([&list](const auto& aList) { list = aList; });
  if (!ret.isOk()) {
    HAL_ERR("get sensors list failed");
    return false;
  }

  // setup the available sensors list and filter out unnecessary ones
  const size_t count = list.size();
  for (size_t i = 0; i < count; i++) {
    const SensorInfo sensorInfo = list[i];

    hal::SensorType sensorType = getSensorType(sensorInfo.type);
    SensorFlagBits mode = (SensorFlagBits)(sensorInfo.flags &
                                           SensorFlagBits::MASK_REPORTING_MODE);
    bool canWakeUp = sensorInfo.flags & SensorFlagBits::WAKE_UP;
    bool isValid = false;

    // check sensor reporting mode and wake-up capability
    switch (sensorType) {
      case SENSOR_PROXIMITY:
        if (mode == SensorFlagBits::ON_CHANGE_MODE && canWakeUp) {
          isValid = true;
        }
        break;
      case SENSOR_LIGHT:
        if (mode == SensorFlagBits::ON_CHANGE_MODE && !canWakeUp) {
          isValid = true;
        }
        break;
      case SENSOR_ORIENTATION:
      case SENSOR_ACCELERATION:
      case SENSOR_GYROSCOPE:
      case SENSOR_LINEAR_ACCELERATION:
      case SENSOR_ROTATION_VECTOR:
      case SENSOR_GAME_ROTATION_VECTOR:
      case SENSOR_PRESSURE:
        if (mode == SensorFlagBits::CONTINUOUS_MODE && !canWakeUp) {
          isValid = true;
        }
        break;
      default:
        break;
    }

    if (isValid) {
      mSensorInfoList[sensorType] = sensorInfo;
      HAL_LOG("a valid sensor type=%d handle=%d is initialized", sensorType,
              sensorInfo.sensorHandle);
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

  if (mSensors == nullptr) {
    return false;
  }

  bool hasSensor = (bool)mSensorInfoList[aSensorType].type;

  // check if specified sensor is supported
  if (!hasSensor) {
    HAL_LOG("device unsupported sensor aSensorType=%d", aSensorType);
    return false;
  }

  const int32_t handle = mSensorInfoList[aSensorType].sensorHandle;

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

  int64_t minDelayNs = mSensorInfoList[aSensorType].minDelay * 1000;
  if (samplingPeriodNs < minDelayNs) {
    samplingPeriodNs = minDelayNs;
  }

  // config sampling period and reporting latency to specified sensor
  auto ret = mSensors->batch(handle, samplingPeriodNs, kReportLatencyNs);
  if (!ret.isOk() || ret != V1_0::Result::OK) {
    HAL_ERR("sensors batch failed aSensorType=%d", aSensorType);
    return false;
  }

  // activate specified sensor
  ret = mSensors->activate(handle, true);
  if (!ret.isOk() || ret != V1_0::Result::OK) {
    HAL_ERR("sensors activate failed aSensorType=%d", aSensorType);
    return false;
  }

  return true;
}

bool GonkSensorsHal::DeactivateSensor(const hal::SensorType aSensorType) {
  MutexAutoLock lock(mLock);

  if (mSensors == nullptr) {
    return false;
  }

  bool hasSensor = (bool)mSensorInfoList[aSensorType].type;

  // check if specified sensor is supported
  if (!hasSensor) {
    HAL_LOG("device unsupported sensor aSensorType=%d", aSensorType);
    return false;
  }

  const int32_t handle = mSensorInfoList[aSensorType].sensorHandle;

  // deactivate specified sensor
  auto ret = mSensors->activate(handle, false);
  if (!ret.isOk() || ret != V1_0::Result::OK) {
    HAL_ERR("sensors deactivate failed aSensorType=%d", aSensorType);
    return false;
  }

  return true;
}

void GonkSensorsHal::GetSensorVendor(const hal::SensorType aSensorType,
                                     nsACString& aRetval) {
  SensorInfo& sensorInfo = mSensorInfoList[aSensorType];
  aRetval.AssignASCII(sensorInfo.vendor.c_str(), sensorInfo.vendor.size());
}

void GonkSensorsHal::GetSensorName(const hal::SensorType aSensorType,
                                   nsACString& aRetval) {
  SensorInfo& sensorInfo = mSensorInfoList[aSensorType];
  aRetval.AssignASCII(sensorInfo.name.c_str(), sensorInfo.name.size());
}

void GonkSensorsHal::PrepareForReconnect() {
  mToReconnect = true;
  if (mSensors->supportsMessageQueues()) {
    // wake up the poll thread from sleep state
    mEventQueueFlag->wake(asBaseType(INTERNAL_WAKE));
  }
}

void GonkSensorsHal::Reconnect() {
  MutexAutoLock lock(mLock);

  HAL_LOG("reconnect sensors hidl server");
  Init();

  // TODO: to recover sensors active/inactive state, tracked by Bug 124978

  mToReconnect = false;
}

}  // namespace hal_impl
}  // namespace mozilla
