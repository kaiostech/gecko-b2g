/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef HAL_GONK_GONKSENSORSHAL_H_
#define HAL_GONK_GONKSENSORSHAL_H_

#include "mozilla/Mutex.h"
#include "nsThreadUtils.h"
#include "HalSensor.h"

#include "android/hardware/sensors/1.0/types.h"
#include "android/hardware/sensors/2.0/types.h"
#include "android_sensors/ISensorsWrapper.h"
#include "fmq/MessageQueue.h"

using namespace mozilla::hal;
using namespace android::hardware::sensors;
using namespace android::SensorServiceUtil;
using android::hardware::Void;
using android::hardware::hidl_vec;
using android::hardware::kSynchronizedReadWrite;
using android::hardware::MessageQueue;
using android::hardware::EventFlag;
using android::hardware::sensors::V1_0::Event;
using android::hardware::sensors::V1_0::SensorInfo;
using android::hardware::sensors::V1_0::SensorFlagBits;
using android::hardware::sensors::V2_0::EventQueueFlagBits;

#define MAX_EVENT_BUFFER_SIZE 128

#ifdef PRODUCT_MANUFACTURER_MTK
// mtk custom hal sends 128 events at most at a time in case data flooding. To avoid
// fmq blocking, the buffer size is enlarged to 128 here specifically for mtk hal as
// a temporary workaround before mtk fix it.
// TODO: to remove this workaround once mtk fix the issue. Track it by Bug 124274.
#undef MAX_EVENT_BUFFER_SIZE
#define MAX_EVENT_BUFFER_SIZE 128
#endif

namespace mozilla {
namespace hal_impl {

typedef void (* SensorDataCallback)(const SensorData& aSensorData);

class SensorsHalDeathRecipient : public android::hardware::hidl_death_recipient {
public:
  virtual void serviceDied(uint64_t cookie, const android::wp<::android::hidl::base::V1_0::IBase>& service) override;
};

class GonkSensorsHal {
public:
  static GonkSensorsHal* GetInstance() {
    if (sInstance == nullptr) {
      sInstance = new GonkSensorsHal();
    }
    return sInstance;
  };

  bool RegisterSensorDataCallback(const SensorDataCallback aCallback);
  bool ActivateSensor(const SensorType aSensorType);
  bool DeactivateSensor(const SensorType aSensorType);
  void GetSensorVendor(const SensorType aSensorType, nsACString& aRetval);
  void GetSensorName(const SensorType aSensorType, nsACString& aRetval);
  void PrepareForReconnect();
  void Reconnect();
private:
  class SensorDataNotifier;

  static GonkSensorsHal* sInstance;

  GonkSensorsHal()
    : mSensors(nullptr),
      mPollThread(nullptr),
      mLock("Sensors"),
      mSensorDataCallback(nullptr),
      mEventQueueFlag(nullptr),
      mToReconnect(false) {
        memset(mSensorInfoList, 0, sizeof(mSensorInfoList));
        Init();
  };
  ~GonkSensorsHal() {};

  void Init();
  bool InitHidlService();
  bool InitHidlServiceV1_0(android::sp<V1_0::ISensors> aServiceV1_0);
  bool InitHidlServiceV2_0(android::sp<V2_0::ISensors> aServiceV2_0);
  bool InitSensorsList();
  void StartPollingThread();
  size_t PollHal();
  size_t PollFmq();
  SensorData CreateSensorData(const Event aEvent);

  android::sp<ISensorsWrapper> mSensors;
  SensorInfo mSensorInfoList[NUM_SENSOR_TYPE];
  nsCOMPtr<nsIThread> mPollThread;
  Mutex mLock;
  SensorDataCallback mSensorDataCallback;

  std::array<Event, MAX_EVENT_BUFFER_SIZE> mEventBuffer;
  typedef MessageQueue<Event, kSynchronizedReadWrite> EventMessageQueue;
  std::unique_ptr<EventMessageQueue> mEventQueue;
  typedef MessageQueue<uint32_t, kSynchronizedReadWrite> WakeLockQueue;
  std::unique_ptr<WakeLockQueue> mWakeLockQueue;
  EventFlag* mEventQueueFlag;

  android::sp<SensorsHalDeathRecipient> mSensorsHalDeathRecipient;
  std::atomic<bool> mToReconnect;

  const int64_t kDefaultSamplingPeriodNs = 200000000;
  const int64_t kPressureSamplingPeriodNs = 1000000000;
  const int64_t kReportLatencyNs = 0;
};

GonkSensorsHal* GonkSensorsHal::sInstance = nullptr;


} // hal_impl
} // mozilla

#endif // HAL_GONK_GONKSENSORSHAL_H_
