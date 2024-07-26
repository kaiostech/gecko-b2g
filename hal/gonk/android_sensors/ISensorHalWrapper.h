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

#ifndef ANDROID_ISENSOR_HAL_WRAPPER_H
#define ANDROID_ISENSOR_HAL_WRAPPER_H

#include <hardware/sensors.h>
#include <stdint.h>
#include <sys/types.h>

namespace android {

/**
 * A wrapper for various types of HAL implementation, e.g. to distinguish HIDL and AIDL versions.
 */
class ISensorHalWrapper {
public:
    class SensorDeviceCallback {
    public:
        virtual void onDynamicSensorsConnected(
                const std::vector<sensor_t> &dynamicSensorsAdded) = 0;

        virtual void onDynamicSensorsDisconnected(
                const std::vector<int32_t> &dynamicSensorHandlesRemoved) = 0;

        virtual ~SensorDeviceCallback(){};
    };

    virtual ~ISensorHalWrapper(){};

    /**
     * Connects to the underlying sensors HAL. This should also be used for any reconnections
     * due to HAL resets.
     */
    virtual bool connect(SensorDeviceCallback *callback) = 0;

    virtual void prepareForReconnect() = 0;

    virtual bool supportsPolling() = 0;

    virtual bool supportsMessageQueues() = 0;

    /**
     * Polls for available sensor events. This could be using the traditional sensors
     * polling or from a FMQ.
     */
    virtual ssize_t poll(sensors_event_t *buffer, size_t count) = 0;

    virtual ssize_t pollFmq(sensors_event_t *buffer, size_t maxNumEventsToRead) = 0;

    /**
     * The below functions directly mirrors the sensors HAL definitions.
     */
    virtual std::vector<sensor_t> getSensorsList() = 0;

    virtual status_t activate(int32_t sensorHandle, bool enabled) = 0;

    virtual status_t batch(int32_t sensorHandle, int64_t samplingPeriodNs,
                           int64_t maxReportLatencyNs) = 0;

    virtual status_t flush(int32_t sensorHandle) = 0;

    std::atomic_bool mReconnecting = false;
};

} // namespace android

#endif // ANDROID_ISENSOR_HAL_WRAPPER_H
