/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MozMtpServer.h"
#include "MozMtpStorage.h"
#include "volume/VolumeManager.h"
#include "MozMtpDatabase.h"

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#include <cutils/properties.h>
#include <private/android_filesystem_config.h>

#include "base/message_loop.h"
#include "DeviceStorage.h"
#include "mozilla/LazyIdleThread.h"
#include "mozilla/Scoped.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsISupportsImpl.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"

#include "volume/Volume.h"
/* C++11 has a builtin char16_t type. */
#define MOZ_UTF16_HELPER(s) u##s

#define MOZ_UTF16(s) MOZ_UTF16_HELPER(s)

#define DEFAULT_THREAD_TIMEOUT_MS 30000
#define MTP_STATE_STARTED MOZ_UTF16("started")
#define MTP_STATE_FINISHED MOZ_UTF16("finished")

using namespace android;
using namespace mozilla;
BEGIN_MTP_NAMESPACE

static const char* kMtpWatcherUpdate = "mtp-watcher-update";
static const char* kMtpStateChanged = "mtp-state-changed";
StaticRefPtr<MozMtpServer> sMozMtpServer;

class MtpWatcherUpdateRunnable final : public Runnable {
 public:
  explicit MtpWatcherUpdateRunnable(MozMtpDatabase* aMozMtpDatabase,
                                    RefCountedMtpServer* aMtpServer,
                                    DeviceStorageFile* aFile,
                                    const nsACString& aEventType)
      : Runnable("MtpWatcherUpdate"),
        mMozMtpDatabase(aMozMtpDatabase),
        mMtpServer(aMtpServer),
        mFile(aFile),
        mEventType(aEventType) {}

  NS_IMETHOD Run() {
    // Runs on the MtpWatcherUpdate->mIOThread
    MOZ_ASSERT(!NS_IsMainThread());

    mMozMtpDatabase->MtpWatcherUpdate(mMtpServer, mFile, mEventType);
    return NS_OK;
  }

 private:
  RefPtr<MozMtpDatabase> mMozMtpDatabase;
  RefPtr<RefCountedMtpServer> mMtpServer;
  RefPtr<DeviceStorageFile> mFile;
  nsCString mEventType;
};

// The MtpWatcherUpdate class listens for mtp-watcher-update events
// and tells the MtpServer about changes made in device storage.
class MtpWatcherUpdate final : public nsIObserver {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  MtpWatcherUpdate(MozMtpServer* aMozMtpServer) : mMozMtpServer(aMozMtpServer) {
    MOZ_ASSERT(NS_IsMainThread());

    mIOThread =
        new LazyIdleThread(DEFAULT_THREAD_TIMEOUT_MS, "MtpWatcherUpdate");

    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    obs->AddObserver(this, kMtpWatcherUpdate, false);
  }

  NS_IMETHOD
  Observe(nsISupports* aSubject, const char* aTopic,
          const char16_t* aData) override {
    MOZ_ASSERT(NS_IsMainThread());

    if (strcmp(aTopic, kMtpWatcherUpdate)) {
      // We're only interested in mtp-watcher-update events
      return NS_OK;
    }

    NS_ConvertUTF16toUTF8 eventType(aData);
    if (!eventType.EqualsLiteral("modified") &&
        !eventType.EqualsLiteral("deleted")) {
      // Bug 1074604: Needn't handle "created" event, once file operation
      // finished, it would trigger "modified" event.
      return NS_OK;
    }

    DeviceStorageFile* file = static_cast<DeviceStorageFile*>(aSubject);
    file->Dump(kMtpWatcherUpdate);
    MTP_LOG("%s: file %s %s", kMtpWatcherUpdate,
            NS_LossyConvertUTF16toASCII(file->mPath).get(), eventType.get());

    RefPtr<MozMtpDatabase> mozMtpDatabase = mMozMtpServer->GetMozMtpDatabase();
    RefPtr<RefCountedMtpServer> mtpServer = mMozMtpServer->GetMtpServer();

    // We're not supposed to perform I/O on the main thread, so punt the
    // notification (which will write to /dev/mtp_usb) to an I/O Thread.

    RefPtr<MtpWatcherUpdateRunnable> r = new MtpWatcherUpdateRunnable(
        mozMtpDatabase, mtpServer, file, eventType);
    mIOThread->Dispatch(r, NS_DISPATCH_NORMAL);

    return NS_OK;
  }

 protected:
  ~MtpWatcherUpdate() {
    MOZ_ASSERT(NS_IsMainThread());

    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    obs->RemoveObserver(this, kMtpWatcherUpdate);
  }

 private:
  RefPtr<MozMtpServer> mMozMtpServer;
  RefPtr<LazyIdleThread> mIOThread;
};
NS_IMPL_ISUPPORTS(MtpWatcherUpdate, nsIObserver)
static StaticRefPtr<MtpWatcherUpdate> sMtpWatcherUpdate;

class AllocMtpWatcherUpdateRunnable final : public Runnable {
 public:
  explicit AllocMtpWatcherUpdateRunnable(MozMtpServer* aMozMtpServer)
      : Runnable("AllocMtpWatcherUpdate"), mMozMtpServer(aMozMtpServer) {}

  NS_IMETHOD Run() {
    MOZ_ASSERT(NS_IsMainThread());

    sMtpWatcherUpdate = new MtpWatcherUpdate(mMozMtpServer);
    return NS_OK;
  }

 private:
  RefPtr<MozMtpServer> mMozMtpServer;
};

class FreeMtpWatcherUpdateRunnable final : public Runnable {
 public:
  explicit FreeMtpWatcherUpdateRunnable(MozMtpServer* aMozMtpServer)
      : Runnable("FreeMtpWatcherUpdate"), mMozMtpServer(aMozMtpServer) {}

  NS_IMETHOD Run() {
    MOZ_ASSERT(NS_IsMainThread());

    sMtpWatcherUpdate = nullptr;
    return NS_OK;
  }

 private:
  RefPtr<MozMtpServer> mMozMtpServer;
};

class MtpStateChangedRunnable final : public Runnable {
 public:
  explicit MtpStateChangedRunnable(const char16_t* aData)
      : Runnable("MtpStateChanged"), mData(aData) {}

  NS_IMETHOD Run() {
    MOZ_ASSERT(NS_IsMainThread());

    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    obs->NotifyObservers(nullptr, kMtpStateChanged, mData);
    return NS_OK;
  }

 private:
  const char16_t* mData;
};
class MtpServerRunnable : public Runnable {
 public:
  explicit MtpServerRunnable(MozMtpServer* aMozMtpServer)
      : Runnable("MtpServerRunnable"), mMozMtpServer(aMozMtpServer) {}

  nsresult Run() {
    RefPtr<RefCountedMtpServer> server = mMozMtpServer->GetMtpServer();

    DebugOnly<nsresult> rv = NS_DispatchToMainThread(
        new AllocMtpWatcherUpdateRunnable(mMozMtpServer));
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    rv =
        NS_DispatchToMainThread(new MtpStateChangedRunnable(MTP_STATE_STARTED));
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    MTP_LOG("MozMtpServer started");

    typedef nsTArray<RefPtr<Volume>> VolumeArray;
    VolumeArray::index_type volIndex;
    VolumeArray::size_type numVolumes = VolumeManager::NumVolumes();
    for (volIndex = 0; volIndex < numVolumes; volIndex++) {
      RefPtr<Volume> vol = VolumeManager::GetVolume(volIndex);
      RefPtr<MozMtpStorage> storage = new MozMtpStorage(vol, sMozMtpServer);
      mMozMtpStorage.AppendElement(storage);
    }
    // server->run hold this until disable mtp function by user.
    server->run();

    MTP_LOG("MozMtpServer finished");
    mMozMtpStorage.Clear();

    rv = NS_DispatchToMainThread(
        new FreeMtpWatcherUpdateRunnable(mMozMtpServer));
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    rv = NS_DispatchToMainThread(
        new MtpStateChangedRunnable(MTP_STATE_FINISHED));
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    return NS_OK;
  }

 private:
  RefPtr<MozMtpServer> mMozMtpServer;
  MozMtpStorage::Array mMozMtpStorage;
};

already_AddRefed<RefCountedMtpServer> MozMtpServer::GetMtpServer() {
  RefPtr<RefCountedMtpServer> server = mMtpServer;
  return server.forget();
}

already_AddRefed<MozMtpDatabase> MozMtpServer::GetMozMtpDatabase() {
  RefPtr<MozMtpDatabase> db = mMozMtpDatabase;
  return db.forget();
}

bool MozMtpServer::Init() {
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());
  char propValueManufacturer[PROPERTY_VALUE_MAX];
  char propValueModel[PROPERTY_VALUE_MAX];
  char propValueDeviceVersion[PROPERTY_VALUE_MAX];
  char propValueSerialNumber[PROPERTY_VALUE_MAX];

  property_get("ro.product.manufacturer", propValueManufacturer, "");
  property_get("ro.product.model", propValueModel, "");
  property_get("ro.version", propValueDeviceVersion, "");
  property_get("ro.serialno", propValueSerialNumber, "");

  #if ANDROID_VERSION < 33
  // FIXME
  mMozMtpDatabase = new MozMtpDatabase();
  #endif
  mMtpServer = new RefCountedMtpServer(
      mMozMtpDatabase.get(),   // IMtpDatabase
      0,                       // controlFd
      false,                   // ptp?
      propValueManufacturer,   // deviceInfoManufacturer
      propValueModel,          // deviceInfoModel
      propValueDeviceVersion,  // deviceInfoDeviceVersion
      propValueSerialNumber);  // deviceInfoSerialNumber
  return true;
}

void MozMtpServer::Run() {
  nsresult rv = NS_NewNamedThread("MtpServer", getter_AddRefs(mServerThread));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }
  MOZ_ASSERT(mServerThread);
  mServerThread->Dispatch(new MtpServerRunnable(this), NS_DISPATCH_NORMAL);
}

END_MTP_NAMESPACE
