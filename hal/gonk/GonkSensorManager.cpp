/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include "mozilla/ClearOnShutdown.h"

#include <android/log.h>

#define SENSOR_LOG_TAG "HidlSensorManager"

#define SENSOR_LOG(args...) \
  __android_log_print(ANDROID_LOG_INFO, SENSOR_LOG_TAG, args)
#define SENSOR_WARN(args...) \
  __android_log_print(ANDROID_LOG_WARN, SENSOR_LOG_TAG, args)
#define SENSOR_INFO(args...) \
  __android_log_print(ANDROID_LOG_INFO, SENSOR_LOG_TAG, args)
#define SENSOR_ERR(args...) \
  __android_log_print(ANDROID_LOG_ERROR, SENSOR_LOG_TAG, args)

#include "GonkSensorManager.h"

#include <sched.h>

#include "EventQueue.h"
#include "DirectReportChannel.h"
#include "utils.h"

#include <binder/IPCThreadState.h>
#include <utils/String8.h>

namespace android {
namespace frameworks {
namespace sensorservice {
namespace V1_0 {
namespace implementation {

using android::hardware::hidl_vec;
using android::hardware::Void;
using android::hardware::sensors::V1_0::SensorInfo;
using android::hardware::sensors::V1_0::SensorsEventFormatOffset;

static const char* POLL_THREAD_NAME = "hidl_ssvc_poll";

SensorManager::SensorManager()
    : mLooper(new Looper(false /*allowNonCallbacks*/)), mStopThread(true) {}

SensorManager::~SensorManager() {
  // Stops pollAll inside the thread.
  std::lock_guard<std::mutex> lock(mThreadMutex);

  mStopThread = true;
  if (mLooper != nullptr) {
    mLooper->wake();
  }
  if (mPollThread.joinable()) {
    mPollThread.join();
  }

  android::SensorManager::removeInstanceForPackage(
      String16(ISensorManager::descriptor));
}

// Methods from ::android::frameworks::sensorservice::V1_0::ISensorManager
// follow.
Return<void> SensorManager::getSensorList(getSensorList_cb _hidl_cb) {
  android::Sensor const* const* list;
  ssize_t count = getInternalManager().getSensorList(&list);
  if (count < 0 || !list) {
    SENSOR_ERR("android::SensorManager::getSensorList encounters count=%zd",
               count);
    _hidl_cb({}, Result::UNKNOWN_ERROR);
    return Void();
  }
  hidl_vec<SensorInfo> ret;
  ret.resize(static_cast<size_t>(count));
  for (ssize_t i = 0; i < count; ++i) {
    ret[i] = convertSensor(*list[i]);
  }
  _hidl_cb(ret, Result::OK);
  return Void();
}

Return<void> SensorManager::getDefaultSensor(SensorType type,
                                             getDefaultSensor_cb _hidl_cb) {
  android::Sensor const* sensor =
      getInternalManager().getDefaultSensor(static_cast<int>(type));
  if (!sensor) {
    _hidl_cb({}, Result::NOT_EXIST);
    return Void();
  }
  _hidl_cb(convertSensor(*sensor), Result::OK);
  return Void();
}

template <typename Callback>
void createDirectChannel(android::SensorManager& manager, size_t size, int type,
                         const native_handle_t* handle,
                         const Callback& _hidl_cb) {
  int channelId = manager.createDirectChannel(size, type, handle);
  if (channelId < 0) {
    _hidl_cb(nullptr, convertResult(channelId));
    return;
  }
  if (channelId == 0) {
    _hidl_cb(nullptr, Result::UNKNOWN_ERROR);
    return;
  }

  _hidl_cb(
      sp<IDirectReportChannel>(new DirectReportChannel(manager, channelId)),
      Result::OK);
}

Return<void> SensorManager::createAshmemDirectChannel(
    const hidl_memory& mem, uint64_t size,
    createAshmemDirectChannel_cb _hidl_cb) {
  if (size > mem.size() ||
      size < (uint64_t)SensorsEventFormatOffset::TOTAL_LENGTH) {
    _hidl_cb(nullptr, Result::BAD_VALUE);
    return Void();
  }

  createDirectChannel(getInternalManager(), size, SENSOR_DIRECT_MEM_TYPE_ASHMEM,
                      mem.handle(), _hidl_cb);

  return Void();
}

Return<void> SensorManager::createGrallocDirectChannel(
    const hidl_handle& buffer, uint64_t size,
    createGrallocDirectChannel_cb _hidl_cb) {
  createDirectChannel(getInternalManager(), size,
                      SENSOR_DIRECT_MEM_TYPE_GRALLOC, buffer.getNativeHandle(),
                      _hidl_cb);

  return Void();
}

/* One global looper for all event queues created from this SensorManager. */
sp<Looper> SensorManager::getLooper() {
  std::lock_guard<std::mutex> lock(mThreadMutex);

  if (!mPollThread.joinable()) {
    // if thread not initialized, start thread
    mStopThread = false;
    std::thread pollThread{[&stopThread = mStopThread, looper = mLooper] {
      struct sched_param p = {0};
      p.sched_priority = 10;
      if (sched_setscheduler(0 /* current thread*/, SCHED_FIFO, &p) != 0) {
        SENSOR_WARN("Could not use SCHED_FIFO for looper thread: %s",
                    strerror(errno));
      }

      // set looper
      Looper::setForThread(looper);

      SENSOR_INFO("%s started", POLL_THREAD_NAME);
      for (;;) {
        int pollResult = looper->pollAll(-1 /* timeout */);
        if (pollResult == Looper::POLL_WAKE) {
          if (stopThread == true) {
            SENSOR_INFO("%s: requested to stop", POLL_THREAD_NAME);
            break;
          } else {
            SENSOR_INFO("%s: spurious wake up, back to work", POLL_THREAD_NAME);
          }
        } else {
          SENSOR_ERR("%s : Looper::pollAll returns unexpected %d",
                     POLL_THREAD_NAME, pollResult);
          break;
        }
      }

      SENSOR_INFO("%s is terminated", POLL_THREAD_NAME);
    }};
    mPollThread = std::move(pollThread);
  }
  return mLooper;
}

::android::SensorManager& SensorManager::getInternalManager() {
  std::lock_guard<std::mutex> lock(mInternalManagerMutex);
  if (mInternalManager == nullptr) {
    SENSOR_INFO("SensorManager::getInternalManager about to get new manager.");
    mInternalManager = &::android::SensorManager::getInstanceForPackage(
        String16(ISensorManager::descriptor));
    SENSOR_INFO("SensorManager::getInternalManager mInternalManager=%p",
                mInternalManager);
  }
  return *mInternalManager;
}

Return<void> SensorManager::createEventQueue(
    const sp<IEventQueueCallback>& callback, createEventQueue_cb _hidl_cb) {
  if (callback == nullptr) {
    _hidl_cb(nullptr, Result::BAD_VALUE);
    return Void();
  }

  sp<::android::Looper> looper = getLooper();
  if (looper == nullptr) {
    SENSOR_ERR("createEventQueue cannot initialize looper");
    _hidl_cb(nullptr, Result::UNKNOWN_ERROR);
    return Void();
  }

  String8 package(String8::format("hidl_client_pid_%d",
                                  IPCThreadState::self()->getCallingPid()));
  sp<::android::SensorEventQueue> internalQueue =
      getInternalManager().createEventQueue(package);
  if (internalQueue == nullptr) {
    SENSOR_WARN("createEventQueue returns nullptr.");
    _hidl_cb(nullptr, Result::UNKNOWN_ERROR);
    return Void();
  }

  sp<IEventQueue> queue = new EventQueue(callback, looper, internalQueue);
  _hidl_cb(queue, Result::OK);

  return Void();
}

StaticRefPtr<SensorManager> gSensorManager;

already_AddRefed<SensorManager> SensorManager::FactoryCreate() {
  SENSOR_LOG("FactoryCreate current=%p", gSensorManager.get());
  MOZ_ASSERT(NS_IsMainThread());

  if (!gSensorManager) {
    gSensorManager = new SensorManager();
    ClearOnShutdown(&gSensorManager);
  }

  // Here register as a hidl service.
  RefPtr<SensorManager> service = gSensorManager;
  if (service->registerAsService() != android::OK) {
    SENSOR_LOG("Failed to register HIDL service.");
  } else {
    SENSOR_LOG("HIDL service registered.");
  }

  return service.forget();
}

NS_IMPL_ISUPPORTS(SensorManager, nsIObserver)

NS_IMETHODIMP
SensorManager::Observe(nsISupports* aSubject, const char* aTopic,
                       const char16_t* aData) {
  // No implementation needed, this is only called as a side effect of
  // the 'profile-after-change' category registration.
  return NS_OK;
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace sensorservice
}  // namespace frameworks
}  // namespace android
