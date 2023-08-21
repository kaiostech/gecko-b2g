/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <android/log.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include "nsIObserverService.h"
#include "nsIThread.h"
#include "nsThreadUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Services.h"

#define LOG(args...) \
  __android_log_print(ANDROID_LOG_INFO, "GonkMemoryPressure", ##args)

namespace {

/**
 * Watch on /dev/socket/kickgccc socket to receive memory pressure events.
 *
 * To keep consistent with b2gkillerd, b2gkillerd will send events to
 * /dev/socket/kickgccc, and the isntance of this class will notify
 * the observers of the "memory-pressure" topic, including GC & CC, to
 * reclaim memory.
 */
class MemoryPressureWatcher final : public nsIRunnable {
  static constexpr char kKickSocketPath[] = "/dev/socket/kickgccc";

 public:
  MemoryPressureWatcher() {
    // Remove the socket if there is, or the address can't be reused.
    unlink(kKickSocketPath);

    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, kKickSocketPath, sizeof(kKickSocketPath));
    mServerFd = socket(AF_UNIX, SOCK_DGRAM, 0);
    MOZ_ASSERT(mServerFd >= 0, "fail to create a socket");
    mozilla::DebugOnly<int> r =
        bind(mServerFd, (struct sockaddr*)&addr, sizeof(addr));
    MOZ_ASSERT(r == 0, "can't bind to kickgccc socket");
  }

  NS_DECL_THREADSAFE_ISUPPORTS

  NS_IMETHOD Run() override {
    MOZ_ASSERT(!NS_IsMainThread());
    while (true) {
      char buf[128];
      mozilla::DebugOnly<int> sz = recv(mServerFd, buf, 128, 0);
      MOZ_ASSERT(sz == sizeof(int), "invalid message size");
      // The content of the message is a pid.  It is not used now, but
      // will be used to specify which process to notify later.
      MOZ_ASSERT(*(int*)buf == getpid(), "invalid PID");
      nsCOMPtr<nsIRunnable> notify = mozilla::NewRunnableMethod(
          "NotifyMemoryPressure", this,
          &MemoryPressureWatcher::NotifyMemoryPressure);
      NS_DispatchToMainThread(notify);
    }
  }

 private:
  ~MemoryPressureWatcher() {}

  void NotifyMemoryPressure() {
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (os) {
      os->NotifyObservers(nullptr, "memory-pressure", u"low-memory");
      LOG("The observers of 'memory-pressure' are notified.");
    } else {
      NS_WARNING("Can't get observer service!");
    }
  }

  int mServerFd;
};

NS_IMPL_ISUPPORTS(MemoryPressureWatcher, nsIRunnable);

}  // namespace

namespace mozilla {

void InitGonkMemoryPressureMonitoring() {
  RefPtr<MemoryPressureWatcher> memoryPressureWatcher =
      new MemoryPressureWatcher();
  nsCOMPtr<nsIThread> thread;
  NS_NewNamedThread("MemoryPressure", getter_AddRefs(thread),
                    memoryPressureWatcher);
}

}  // namespace mozilla
