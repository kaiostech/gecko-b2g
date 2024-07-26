/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* Copyright 2012 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "Hal.h"
#include "base/task.h"

#if ANDROID_VERSION >= 34
#include "gonk/GonkSensorsHal.h"
#else
#include "gonk/old/GonkSensorsHal.h"
#endif

using namespace mozilla::hal;

namespace mozilla {
namespace hal_impl {

static nsCOMPtr<nsIThread> mSensorsThread = nullptr;

static GonkSensorsHal* sSensorsHal = nullptr;

void EnableSensorNotifications(SensorType aSensor) {
  MOZ_ASSERT(NS_IsMainThread());

  if (mSensorsThread == nullptr) {
    nsresult rv =
        NS_NewNamedThread("GonkSensors", getter_AddRefs(mSensorsThread));
    if (NS_FAILED(rv)) {
      mSensorsThread = nullptr;
    }
  }

  if (mSensorsThread) {
    mSensorsThread->Dispatch(
        NS_NewRunnableFunction("ActivateSensor", [aSensor]() {
          if (!sSensorsHal) {
            sSensorsHal = GonkSensorsHal::GetInstance();
          }
          sSensorsHal->ActivateSensor(aSensor);

          static bool isRegistered = false;
          if (!isRegistered) {
            isRegistered =
                sSensorsHal->RegisterSensorDataCallback(&NotifySensorChange);
          }
        }));
  }
}

void DisableSensorNotifications(SensorType aSensor) {
  MOZ_ASSERT(NS_IsMainThread());

  if (mSensorsThread == nullptr) {
    nsresult rv =
        NS_NewNamedThread("GonkSensors", getter_AddRefs(mSensorsThread));
    if (NS_FAILED(rv)) {
      mSensorsThread = nullptr;
    }
  }

  if (mSensorsThread) {
    mSensorsThread->Dispatch(
        NS_NewRunnableFunction("DeactivateSensor", [aSensor]() {
          if (!sSensorsHal) {
            sSensorsHal = GonkSensorsHal::GetInstance();
          }
          sSensorsHal->DeactivateSensor(aSensor);
        }));
  }
}

}  // namespace hal_impl
}  // namespace mozilla
