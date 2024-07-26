/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef HAL_GONK_GONKSENSORSHAL_H_
#define HAL_GONK_GONKSENSORSHAL_H_

#include "mozilla/HalSensor.h"
#include "mozilla/Mutex.h"
#include "nsThreadUtils.h"

#include "gonk/ISensorsWrapper.h"
#include "fmq/MessageQueue.h"

using namespace mozilla::hal;
using namespace android::SensorServiceUtil;
using android::hardware::Void;

namespace mozilla {
namespace hal_impl {

typedef void (*SensorDataCallback)(const SensorData& aSensorData);

class SensorsHalDeathRecipient
    : public android::hardware::hidl_death_recipient {
 public:
  virtual void serviceDied(
      uint64_t cookie,
      const android::wp<::android::hidl::base::V1_0::IBase>& service) override;
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
  bool ActivateSensor(const hal::SensorType aSensorType);
  bool DeactivateSensor(const hal::SensorType aSensorType);
  void GetSensorVendor(const hal::SensorType aSensorType, nsACString& aRetval);
  void GetSensorName(const hal::SensorType aSensorType, nsACString& aRetval);
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
  ~GonkSensorsHal(){};

  void Init();
  bool InitHidlService();
  bool InitHidlServiceV1_0(android::sp<V1_0::ISensors> aServiceV1_0);
  bool InitHidlServiceV2_0(android::sp<V2_0::ISensors> aServiceV2_0);
#ifdef HAL_HIDL_V21
  bool InitHidlServiceV2_1(android::sp<V2_1::ISensors> aServiceV2_1);
#endif
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
  typedef MessageQueue<uint32_t, kSynchronizedReadWrite> WakeLockQueue;
  std::unique_ptr<WakeLockQueue> mWakeLockQueue;
  EventFlag* mEventQueueFlag;

  android::sp<SensorsHalDeathRecipient> mSensorsHalDeathRecipient;
  std::atomic<bool> mToReconnect;

  const int64_t kDefaultSamplingPeriodNs = 200000000;
  const int64_t kPressureSamplingPeriodNs = 100000000;
  const int64_t kReportLatencyNs = 0;
};

}  // namespace hal_impl
}  // namespace mozilla

#endif  // HAL_GONK_GONKSENSORSHAL_H_
