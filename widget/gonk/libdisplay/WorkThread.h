/* Copyright (C) 2020 KAI OS TECHNOLOGIES (HONG KONG) LIMITED. All rights
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

#ifndef CARTHAGE_WORKTHREAD_H
#define CARTHAGE_WORKTHREAD_H

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>

namespace carthage {

class WorkThread {
 public:
  class Work {
   public:
    virtual ~Work(){};
    virtual void Exec() = 0;
    virtual void Destroy() = 0;

    template <typename T>
    static std::shared_ptr<WorkThread::Work> Create(T t) {
      return std::make_shared<WorkThread::LambdaWork<T>>(t);
    }
  };

  template <typename FUNC>
  class LambdaWork : public Work {
   public:
    LambdaWork(FUNC aFunc) : mFunc(aFunc) {}

    virtual void Exec() { mFunc(); }

    virtual void Destroy() {}

   private:
    FUNC mFunc;
  };

  WorkThread();
  virtual ~WorkThread(){};

  virtual void DoPost(std::shared_ptr<Work> aWork);

  template <typename T>
  void Post(T t) {
    DoPost(Work::Create<T>(t));
  }

  // to notifiy a thread exit the internal ThreadLoop;
  void SendExitSignal();

  // to wait a thread finish
  void Join();

  // detach a thread, the handle is invalid after this call.
  void Detach();

 protected:
  // the "main" function for thread
  void ThreadLoop();

  std::shared_ptr<Work> GetNext();

  bool mExiting;
  std::thread mThread;
  std::mutex mMutex;
  std::condition_variable mCV;
  std::queue<std::shared_ptr<Work>> mQueue;
};

}  // namespace carthage

#endif
