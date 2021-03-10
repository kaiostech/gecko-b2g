/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/input.h>
#include "nsIObserver.h"
#include "nsIObserverService.h"

// Minimum time for the vibration to be felt on the pinephone, in ms. :(
#define MIN_VIBRATION_DURATION 50

namespace mozilla::hal_impl {

namespace {

/**
 * This runnable runs for the lifetime of the program, once started.  It's
 * responsible for "playing" vibration patterns.
 */
class VibratorRunnable final : public nsIRunnable, public nsIObserver {
 public:
  VibratorRunnable()
      : mMonitor("VibratorRunnable"), mIndex(0), mFd(-1), mEffectId(-1) {
    nsCOMPtr<nsIObserverService> os = services::GetObserverService();
    if (!os) {
      NS_WARNING("Could not get observer service!");
      return;
    }

    os->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);

    // Iterates over the input event sources to find a vibration capable one.
    char path[256];
    char name[256];
    int fd, ret;

    // find the right device and open it
    for (int i = 0; i < 10; i++) {
      SprintfLiteral(path, "/dev/input/event%d", i);
      fd = open(path, O_RDWR | O_CLOEXEC);
      if (fd < 0) {
        continue;
      }

      ret = ioctl(fd, EVIOCGNAME(256), name);
      if (ret < 0) {
        close(fd);
        continue;
      }

      if (strstr(name, "vibrator")) {
        // Use the first vibration device we find.
        mFd = fd;
        break;
      } else {
        close(fd);
      }
    }
  }

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIRUNNABLE
  NS_DECL_NSIOBSERVER

  // Run on the main thread, not the vibrator thread.
  void Vibrate(const nsTArray<uint32_t>& pattern);
  void CancelVibrate();

  static bool ShuttingDown() { return sShuttingDown; }

 protected:
  ~VibratorRunnable() {
    if (mFd != -1) {
      close(mFd);
    }
  }

  void startVibration(uint32_t duration) {
    if (mFd == -1) {
      return;
    }

    if (duration < MIN_VIBRATION_DURATION) {
      duration = MIN_VIBRATION_DURATION;
    }

    // If a vibration is ongoing, stop it.
    stopVibration();

    int effects;
    if (ioctl(mFd, EVIOCGEFFECTS, &effects) < 0) {
      NS_WARNING("EVIOCGEFFECTS failed");
      return;
    }

    struct ff_effect e = {
        .type = FF_RUMBLE,
        .id = -1,
        .replay =
            {
                .length = static_cast<__u16>(duration),
                .delay = 0,
            },
        .u.rumble =
            {
                .strong_magnitude = 1,
            },
    };

    if (ioctl(mFd, EVIOCSFF, &e) < 0) {
      NS_WARNING("EVIOCSFF failed");
      return;
    }

    struct input_event play = {
        .type = EV_FF, .code = static_cast<__u16>(e.id), .value = 1};

    Unused << write(mFd, &play, sizeof play);
    mEffectId = e.id;
  }

  void stopVibration() {
    if (mFd == -1 || mEffectId == -1) {
      return;
    }

    Unused << ioctl(mFd, EVIOCRMFF, mEffectId);
    mEffectId = -1;
  }

 private:
  Monitor mMonitor;

  // The currently-playing pattern.
  nsTArray<uint32_t> mPattern;

  // The index we're at in the currently-playing pattern.  If mIndex >=
  // mPattern.Length(), then we're not currently playing anything.
  uint32_t mIndex;

  // Set to true in our shutdown observer.  When this is true, we kill the
  // vibrator thread.
  static bool sShuttingDown;

  // The fd of the discovered force feedback input sysfs
  int mFd;
  // The id to reuse when cancelling an existing vibration.
  int mEffectId;
};

NS_IMPL_ISUPPORTS(VibratorRunnable, nsIRunnable, nsIObserver);

bool VibratorRunnable::sShuttingDown = false;

static StaticRefPtr<VibratorRunnable> sVibratorRunnable;

NS_IMETHODIMP
VibratorRunnable::Run() {
  MonitorAutoLock lock(mMonitor);

  // We currently assume that mMonitor.Wait(X) waits for X milliseconds.  But in
  // reality, the kernel might not switch to this thread for some time after the
  // wait expires.  So there's potential for some inaccuracy here.
  //
  // This doesn't worry me too much.  Note that we don't even start vibrating
  // immediately when VibratorRunnable::Vibrate is called -- we go through a
  // condvar onto another thread.  Better just to be chill about small errors in
  // the timing here.

  while (!sShuttingDown) {
    if (mIndex < mPattern.Length()) {
      uint32_t duration = mPattern[mIndex];
      if ((mPattern.Length() == 1) && (duration == 0)) {
        stopVibration();
      } else if (mIndex % 2 == 0) {
        startVibration(duration);
      }
      mIndex++;
      mMonitor.Wait(TimeDuration::FromMilliseconds(duration));
    } else {
      mMonitor.Wait();
    }
  }
  sVibratorRunnable = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
VibratorRunnable::Observe(nsISupports* subject, const char* topic,
                          const char16_t* data) {
  MOZ_ASSERT(strcmp(topic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0);
  MonitorAutoLock lock(mMonitor);
  sShuttingDown = true;
  mMonitor.Notify();

  return NS_OK;
}

void VibratorRunnable::Vibrate(const nsTArray<uint32_t>& pattern) {
  MonitorAutoLock lock(mMonitor);
  mPattern.Assign(pattern);
  mIndex = 0;
  mMonitor.Notify();
}

void VibratorRunnable::CancelVibrate() {
  MonitorAutoLock lock(mMonitor);
  mPattern.Clear();
  mPattern.AppendElement(0);
  mIndex = 0;
  mMonitor.Notify();
}

void EnsureVibratorThreadInitialized() {
  if (sVibratorRunnable) {
    return;
  }

  sVibratorRunnable = new VibratorRunnable();
  nsCOMPtr<nsIThread> thread;
  NS_NewNamedThread("Vibrator", getter_AddRefs(thread), sVibratorRunnable);
}

}  // namespace

void Vibrate(const nsTArray<uint32_t>& pattern, hal::WindowIdentifier&&) {
  MOZ_ASSERT(NS_IsMainThread());
  if (VibratorRunnable::ShuttingDown()) {
    return;
  }
  EnsureVibratorThreadInitialized();
  sVibratorRunnable->Vibrate(pattern);
}

void CancelVibrate(hal::WindowIdentifier&&) {
  MOZ_ASSERT(NS_IsMainThread());
  if (VibratorRunnable::ShuttingDown()) {
    return;
  }
  EnsureVibratorThreadInitialized();
  sVibratorRunnable->CancelVibrate();
}

}  // namespace mozilla::hal_impl
