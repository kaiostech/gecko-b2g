/* Copyright (C) 2021 KAI OS TECHNOLOGIES (HONG KONG) LIMITED. All rights
 * reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef GonkScreenRecord_H_
#define GonkScreenRecord_H_

#include "base/thread.h"
#include "mozilla/TimeStamp.h"
#include "nsThreadUtils.h"

namespace mozilla {

class GonkScreenRecord : public Runnable {
  typedef void (*FinishCallback)(int streamFd, int res);

 public:
  GonkScreenRecord(uint32_t displayId, uint32_t outputFormat,
                   uint32_t timeLimitSec, const char* fileName,
                   FinishCallback finishCallback, int streamFd)
      : mozilla::Runnable("GonkScreenRecord"),
        mDisplayId(displayId),
        mOutputFormat(outputFormat),
        mTimeLimitSec(timeLimitSec),
        mStreamFd(streamFd),
        mFinishCallback(finishCallback),
        mVsyncThread(nullptr) {
    mFileName = (char*)malloc(strlen(fileName) + 1);
    strcpy(mFileName, fileName);
  }

  ~GonkScreenRecord() {
    if (mFileName) {
      free(mFileName);
    }
  }

  NS_IMETHOD
  Run() override { return (nsresult)capture(); }

  void NotifyVsync(const int display, const mozilla::TimeStamp& aVsyncTimestamp,
                   const mozilla::TimeDuration& aVsyncPeriod);

  void ClearVsyncTask();

  int capture();

 private:
  uint32_t mDisplayId;
  uint32_t mOutputFormat;
  uint32_t mTimeLimitSec;
  char* mFileName;
  int mStreamFd;
  FinishCallback mFinishCallback;
  ::base::Thread* mVsyncThread;
  RefPtr<mozilla::CancelableRunnable> mCurrentVsyncTask;
};

} /* namespace mozilla */

#endif /* GonkScreenshot_H_ */
