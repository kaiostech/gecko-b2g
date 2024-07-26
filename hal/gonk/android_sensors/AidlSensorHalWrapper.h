/*
 * Copyright (C) 2021 The Android Open Source Project
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

#ifndef ANDROID_AIDL_SENSOR_HAL_WRAPPER_H
#define ANDROID_AIDL_SENSOR_HAL_WRAPPER_H

#include "ISensorHalWrapper.h"

#include <aidl/android/hardware/sensors/ISensors.h>
#include <fmq/AidlMessageQueue.h>

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
static constexpr size_t MAX_RECEIVE_BUFFER_EVENT_COUNT = MAX_EVENT_BUFFER_SIZE;

namespace android {

class AidlSensorHalWrapper : public ISensorHalWrapper {
public:
    AidlSensorHalWrapper();

    ~AidlSensorHalWrapper() override {
        if (mEventQueueFlag != nullptr) {
            ::android::hardware::EventFlag::deleteEventFlag(&mEventQueueFlag);
            mEventQueueFlag = nullptr;
        }
        if (mWakeLockQueueFlag != nullptr) {
            ::android::hardware::EventFlag::deleteEventFlag(&mWakeLockQueueFlag);
            mWakeLockQueueFlag = nullptr;
        }
    }

    virtual bool connect(SensorDeviceCallback *callback) override;

    virtual void prepareForReconnect() override;

    virtual bool supportsPolling() override;

    virtual bool supportsMessageQueues() override;

    virtual ssize_t poll(sensors_event_t *buffer, size_t count) override;

    virtual ssize_t pollFmq(sensors_event_t *buffer, size_t count) override;

    virtual std::vector<sensor_t> getSensorsList() override;

    virtual status_t activate(int32_t sensorHandle, bool enabled) override;

    virtual status_t batch(int32_t sensorHandle, int64_t samplingPeriodNs,
                           int64_t maxReportLatencyNs) override;

    virtual status_t flush(int32_t sensorHandle) override;

private:
    std::shared_ptr<aidl::android::hardware::sensors::ISensors> mSensors;
    std::shared_ptr<::aidl::android::hardware::sensors::ISensorsCallback> mCallback;
    std::unique_ptr<::android::AidlMessageQueue<::aidl::android::hardware::sensors::Event,
                                                SynchronizedReadWrite>>
            mEventQueue;
    std::unique_ptr<::android::AidlMessageQueue<int, SynchronizedReadWrite>> mWakeLockQueue;
    ::android::hardware::EventFlag *mEventQueueFlag;
    ::android::hardware::EventFlag *mWakeLockQueueFlag;
    SensorDeviceCallback *mSensorDeviceCallback;
    std::array<::aidl::android::hardware::sensors::Event,
               MAX_RECEIVE_BUFFER_EVENT_COUNT>
            mEventBuffer;

    ndk::ScopedAIBinder_DeathRecipient mDeathRecipient;
};

} // namespace android

#endif // ANDROID_AIDL_SENSOR_HAL_WRAPPER_H
