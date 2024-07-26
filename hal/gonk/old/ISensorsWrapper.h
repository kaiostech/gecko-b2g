/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_SENSORS_WRAPPER_H
#define ANDROID_SENSORS_WRAPPER_H

#if ANDROID_VERSION >= 33
#  define HAL_HIDL_V21
#endif

#include "android/hardware/sensors/1.0/ISensors.h"
#include "android/hardware/sensors/1.0/types.h"
#include "android/hardware/sensors/2.0/ISensors.h"
#include "android/hardware/sensors/2.0/ISensorsCallback.h"
#include "android/hardware/sensors/2.0/types.h"

#ifdef HAL_HIDL_V21
#  include "android/hardware/sensors/2.1/ISensors.h"
#  include "android/hardware/sensors/2.1/ISensorsCallback.h"
#  include "android/hardware/sensors/2.1/types.h"
#endif

#include <fmq/MessageQueue.h>
#include <hidl/MQDescriptor.h>
#include <utils/LightRefBase.h>

#define MAX_EVENT_BUFFER_SIZE 16

#ifdef PRODUCT_MANUFACTURER_MTK
// mtk custom hal sends 128 events at most at a time in case data flooding. To
// avoid fmq blocking, the buffer size is enlarged to 128 here specifically for
// mtk hal as a temporary workaround before mtk fix it.
// TODO: to remove this workaround once mtk fix the issue. Track it by Bug
// 124274.
#  undef MAX_EVENT_BUFFER_SIZE
#  define MAX_EVENT_BUFFER_SIZE 128
#endif

namespace android {
namespace SensorServiceUtil {

using android::hardware::EventFlag;
using android::hardware::hidl_vec;
using android::hardware::kSynchronizedReadWrite;
using android::hardware::MessageQueue;
using android::hardware::MQDescriptorSync;
using android::hardware::Return;
using android::hardware::sensors::V1_0::Result;
using android::hardware::sensors::V1_0::SensorFlagBits;
using android::hardware::sensors::V2_0::EventQueueFlagBits;
using namespace android::hardware::sensors;

#ifdef HAL_HIDL_V21
using android::hardware::sensors::V2_1::Event;
using android::hardware::sensors::V2_1::ISensorsCallback;
using android::hardware::sensors::V2_1::SensorInfo;
#else
using android::hardware::sensors::V1_0::Event;
using android::hardware::sensors::V1_0::SensorInfo;
using android::hardware::sensors::V2_0::ISensorsCallback;
#endif

/*
 * The EventMessageQueueWrapperBase interface abstract away the common logic
 * between both the V1.0 and V2.1 versions of the Sensors Event interface. This
 * allows users of these classes to only care about the EventMessageQueue
 * without handling different versions of Events.
 */
class EventMessageQueueWrapperBase {
 public:
  virtual ~EventMessageQueueWrapperBase() {}

  virtual std::atomic<uint32_t>* getEventFlagWord() = 0;
  virtual size_t availableToRead() = 0;
  virtual size_t availableToWrite() = 0;
  virtual bool read(Event* events, size_t numToRead) = 0;
  virtual bool write(const Event* events, size_t numToWrite) = 0;
  virtual bool write(const std::vector<Event>& events) = 0;
  virtual bool writeBlocking(const Event* events, size_t count,
                             uint32_t readNotification,
                             uint32_t writeNotification, int64_t timeOutNanos,
                             EventFlag* evFlag) = 0;
  virtual size_t getQuantumCount() = 0;
};

class EventMessageQueueWrapperV1_0 : public EventMessageQueueWrapperBase {
 public:
  using EventMessageQueue = MessageQueue<V1_0::Event, kSynchronizedReadWrite>;

  EventMessageQueueWrapperV1_0(std::unique_ptr<EventMessageQueue>& queue)
      : mQueue(std::move(queue)) {}

  const MQDescriptorSync<V1_0::Event>* getDesc() { return mQueue->getDesc(); }

  virtual std::atomic<uint32_t>* getEventFlagWord() override {
    return mQueue->getEventFlagWord();
  }

  virtual size_t availableToRead() override {
    return mQueue->availableToRead();
  }

  size_t availableToWrite() override { return mQueue->availableToWrite(); }

#ifdef HAL_HIDL_V21
  // convert V2_1::Event into V1_0::Event for backward compatibility

  virtual bool read(V2_1::Event* events, size_t numToRead) override {
    return mQueue->read(reinterpret_cast<V1_0::Event*>(events), numToRead);
  }

  bool write(const V2_1::Event* events, size_t numToWrite) override {
    return mQueue->write(reinterpret_cast<const V1_0::Event*>(events),
                         numToWrite);
  }

  virtual bool write(const std::vector<V2_1::Event>& events) override {
    const std::vector<V1_0::Event>& oldEvents =
        reinterpret_cast<const std::vector<V1_0::Event>&>(events);
    return mQueue->write(oldEvents.data(), oldEvents.size());
  }

  bool writeBlocking(const V2_1::Event* events, size_t count,
                     uint32_t readNotification, uint32_t writeNotification,
                     int64_t timeOutNanos, EventFlag* evFlag) override {
    return mQueue->writeBlocking(reinterpret_cast<const V1_0::Event*>(events),
                                 count, readNotification, writeNotification,
                                 timeOutNanos, evFlag);
  }
#else
  virtual bool read(V1_0::Event* events, size_t numToRead) override {
    return mQueue->read(events, numToRead);
  }

  bool write(const V1_0::Event* events, size_t numToWrite) override {
    return mQueue->write(events, numToWrite);
  }

  virtual bool write(const std::vector<V1_0::Event>& events) override {
    return mQueue->write(events.data(), events.size());
  }

  bool writeBlocking(const V1_0::Event* events, size_t count,
                     uint32_t readNotification, uint32_t writeNotification,
                     int64_t timeOutNanos, EventFlag* evFlag) override {
    return mQueue->writeBlocking(events, count, readNotification,
                                 writeNotification, timeOutNanos, evFlag);
  }
#endif

  size_t getQuantumCount() override { return mQueue->getQuantumCount(); }

 private:
  std::unique_ptr<EventMessageQueue> mQueue;
};

#ifdef HAL_HIDL_V21
class EventMessageQueueWrapperV2_1 : public EventMessageQueueWrapperBase {
 public:
  using EventMessageQueue = MessageQueue<V2_1::Event, kSynchronizedReadWrite>;

  EventMessageQueueWrapperV2_1(std::unique_ptr<EventMessageQueue>& queue)
      : mQueue(std::move(queue)) {}

  const MQDescriptorSync<V2_1::Event>* getDesc() { return mQueue->getDesc(); }

  std::atomic<uint32_t>* getEventFlagWord() override {
    return mQueue->getEventFlagWord();
  }

  virtual size_t availableToRead() override {
    return mQueue->availableToRead();
  }

  size_t availableToWrite() override { return mQueue->availableToWrite(); }

  virtual bool read(V2_1::Event* events, size_t numToRead) override {
    return mQueue->read(events, numToRead);
  }

  bool write(const V2_1::Event* events, size_t numToWrite) override {
    return mQueue->write(events, numToWrite);
  }

  bool write(const std::vector<V2_1::Event>& events) override {
    return mQueue->write(events.data(), events.size());
  }

  bool writeBlocking(const V2_1::Event* events, size_t count,
                     uint32_t readNotification, uint32_t writeNotification,
                     int64_t timeOutNanos, EventFlag* evFlag) override {
    return mQueue->writeBlocking(events, count, readNotification,
                                 writeNotification, timeOutNanos, evFlag);
  }

  size_t getQuantumCount() override { return mQueue->getQuantumCount(); }

 private:
  std::unique_ptr<EventMessageQueue> mQueue;
};
#endif

/*
 * The ISensorsWrapper interface includes all function from supported Sensors
 * HAL versions. This allows for the SensorDevice to use the ISensorsWrapper
 * interface to interact with the Sensors HAL regardless of the current version
 * of the Sensors HAL that is loaded. Each concrete instantiation of
 * ISensorsWrapper must correspond to a specific Sensors HAL version. This
 * design is beneficial because only the functions that change between Sensors
 * HAL versions must be newly newly implemented, any previously implemented
 * function that does not change may remain the same.
 *
 * Functions that exist across all versions of the Sensors HAL should be
 * implemented as pure virtual functions which forces the concrete
 * instantiations to implement the functions.
 *
 * Functions that do not exist across all versions of the Sensors HAL should
 * include a default implementation that generates an error if called. The
 * default implementation should never be called and must be overridden by
 * Sensors HAL versions that support the function.
 */
class ISensorsWrapper : public VirtualLightRefBase {
 public:
  virtual bool supportsPolling() const = 0;

  virtual bool supportsMessageQueues() const = 0;

#ifdef HAL_HIDL_V21
  virtual Return<void> getSensorsList(
      V2_1::ISensors::getSensorsList_2_1_cb _hidl_cb) = 0;
#else
  virtual Return<void> getSensorsList(
      V1_0::ISensors::getSensorsList_cb _hidl_cb) = 0;
#endif

  virtual Return<Result> activate(int32_t sensorHandle, bool enabled) = 0;

  virtual Return<Result> batch(int32_t sensorHandle, int64_t samplingPeriodNs,
                               int64_t maxReportLatencyNs) = 0;

  virtual Return<void> poll(int32_t maxCount,
                            V1_0::ISensors::poll_cb _hidl_cb) {
    (void)maxCount;
    (void)_hidl_cb;
    return Return<void>();
  }

  virtual EventMessageQueueWrapperBase* getEventQueue() { return nullptr; }

  virtual Return<Result> initialize(
      const MQDescriptorSync<uint32_t>& wakeLockDesc,
      const sp<ISensorsCallback>& callback) {
    (void)wakeLockDesc;
    (void)callback;
    return Result::INVALID_OPERATION;
  }
};

template <typename T>
class SensorsWrapperBase : public ISensorsWrapper {
 public:
  SensorsWrapperBase(sp<T> sensors) : mSensors(sensors){};

#ifdef HAL_HIDL_V21
  virtual Return<void> getSensorsList(
      V2_1::ISensors::getSensorsList_2_1_cb _hidl_cb) override {
    // convert V1_0::SensorInfo into V2_1::SensorInfo
    return mSensors->getSensorsList([&](const auto& list) {
      _hidl_cb(reinterpret_cast<const hidl_vec<V2_1::SensorInfo>&>(list));
    });
  }
#else
  Return<void> getSensorsList(
      V1_0::ISensors::getSensorsList_cb _hidl_cb) override {
    return mSensors->getSensorsList(_hidl_cb);
  }
#endif

  Return<Result> activate(int32_t sensorHandle, bool enabled) override {
    return mSensors->activate(sensorHandle, enabled);
  }

  Return<Result> batch(int32_t sensorHandle, int64_t samplingPeriodNs,
                       int64_t maxReportLatencyNs) override {
    return mSensors->batch(sensorHandle, samplingPeriodNs, maxReportLatencyNs);
  }

 protected:
  sp<T> mSensors;
};

class SensorsWrapperV1_0 : public SensorsWrapperBase<V1_0::ISensors> {
 public:
  SensorsWrapperV1_0(sp<V1_0::ISensors> sensors)
      : SensorsWrapperBase(sensors){};

  bool supportsPolling() const override { return true; }

  bool supportsMessageQueues() const override { return false; }

  Return<void> poll(int32_t maxCount,
                    V1_0::ISensors::poll_cb _hidl_cb) override {
    return mSensors->poll(maxCount, _hidl_cb);
  }
};

class SensorsWrapperV2_0 : public SensorsWrapperBase<V2_0::ISensors> {
 public:
  SensorsWrapperV2_0(sp<V2_0::ISensors> sensors) : SensorsWrapperBase(sensors) {
    typedef MessageQueue<V1_0::Event, kSynchronizedReadWrite> EventMessageQueue;
    auto eventQueue =
        std::make_unique<EventMessageQueue>(MAX_EVENT_BUFFER_SIZE, true);
    mEventQueue = std::make_unique<EventMessageQueueWrapperV1_0>(eventQueue);
  };

  bool supportsPolling() const override { return false; }

  bool supportsMessageQueues() const override { return true; }

  EventMessageQueueWrapperBase* getEventQueue() override {
    return mEventQueue.get();
  }

  Return<Result> initialize(const MQDescriptorSync<uint32_t>& wakeLockDesc,
                            const sp<ISensorsCallback>& callback) override {
    return mSensors->initialize(*mEventQueue->getDesc(), wakeLockDesc,
                                callback);
  }

 private:
  std::unique_ptr<EventMessageQueueWrapperV1_0> mEventQueue;
};

#ifdef HAL_HIDL_V21
class SensorsWrapperV2_1 : public SensorsWrapperBase<V2_1::ISensors> {
 public:
  SensorsWrapperV2_1(sp<V2_1::ISensors> sensors) : SensorsWrapperBase(sensors) {
    typedef MessageQueue<V2_1::Event, kSynchronizedReadWrite>
        EventMessageQueueV2_1;
    auto eventQueue =
        std::make_unique<EventMessageQueueV2_1>(MAX_EVENT_BUFFER_SIZE, true);
    mEventQueue = std::make_unique<EventMessageQueueWrapperV2_1>(eventQueue);
  };

  bool supportsPolling() const override { return false; }

  bool supportsMessageQueues() const override { return true; }

  EventMessageQueueWrapperBase* getEventQueue() override {
    return mEventQueue.get();
  }

  Return<Result> initialize(
      const MQDescriptorSync<uint32_t>& wakeLockDesc,
      const sp<V2_1::ISensorsCallback>& callback) override {
    return mSensors->initialize_2_1(*mEventQueue->getDesc(), wakeLockDesc,
                                    callback);
  }

  Return<void> getSensorsList(
      V2_1::ISensors::getSensorsList_2_1_cb _hidl_cb) override {
    return mSensors->getSensorsList_2_1(_hidl_cb);
  }

 private:
  std::unique_ptr<EventMessageQueueWrapperV2_1> mEventQueue;
};
#endif

};  // namespace SensorServiceUtil
};  // namespace android

#endif  // ANDROID_SENSORS_WRAPPER_H
