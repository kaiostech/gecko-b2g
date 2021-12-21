/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <android/os/IVold.h>
#include <binder/IBinder.h>
#include <binder/IServiceManager.h>
#include <binder/IInterface.h>
#include <binder/IPCThreadState.h>
#include <utils/String16.h>
#include "mozilla/ClearOnShutdown.h"
#include "VoldProxy.h"
#include "VolumeManager.h"
#include "nsXULAppAPI.h"
#include "base/task.h"

#undef USE_DEBUG
#define USE_DEBUG 0

#undef LOG
#undef ERR
#define LOG(args...) __android_log_print(ANDROID_LOG_INFO, "VoldProxy", ##args)
#define ERR(args...) __android_log_print(ANDROID_LOG_ERROR, "VoldProxy", ##args)

#undef DBG
#if USE_DEBUG
#  define DBG(args...) \
    __android_log_print(ANDROID_LOG_DEBUG, "VoldProxy", ##args)
#else
#  define DBG(args...)
#endif

using ::android::defaultServiceManager;
using ::android::IBinder;
using ::android::interface_cast;
using ::android::IServiceManager;
using ::android::sp;
using ::android::String16;
using ::android::binder::Status;
using ::android::os::IVold;

namespace mozilla {
namespace system {

sp<IVold> gVold = nullptr;
mozilla::Mutex VoldListener::sLock("vold-event");
VoldListener* VoldListener::sInstance = nullptr;

VoldListener* VoldListener::CreateInstance() {
  if (sInstance) {
    return sInstance;
  }
  DBG("Create new instance\n", __func__);
  // Create new instance
  sInstance = new VoldListener();

  return sInstance;
}

void VoldListener::CleanUp() {
  if (sInstance) {
    delete sInstance;
    sInstance = nullptr;
  }
}

android::binder::Status VoldListener::onDiskCreated(const ::std::string& diskId,
                                                    int32_t flags) {
  MutexAutoLock lock(sLock);
  DBG("%s, diskId: %s, flags: %d\n", __func__, diskId.c_str(), flags);
  return android::binder::Status::ok();
}

android::binder::Status VoldListener::onDiskScanned(
    const ::std::string& diskId) {
  MutexAutoLock lock(sLock);
  DBG("%s, diskId: %s\n", __func__, diskId.c_str());
  return android::binder::Status::ok();
}

android::binder::Status VoldListener::onDiskMetadataChanged(
    const ::std::string& diskId, int64_t sizeBytes, const ::std::string& label,
    const ::std::string& sysPath) {
  MutexAutoLock lock(sLock);
  DBG("%s, diskId: %s, sizeBytes: %lld, label: %s, sysPath: %s\n", __func__,
      diskId.c_str(), sizeBytes, label.c_str(), sysPath.c_str());
  return android::binder::Status::ok();
}

android::binder::Status VoldListener::onDiskDestroyed(
    const ::std::string& diskId) {
  MutexAutoLock lock(sLock);
  DBG("%s, diskId: %s\n", __func__, diskId.c_str());
  return android::binder::Status::ok();
}

android::binder::Status VoldListener::onVolumeCreated(
    const ::std::string& volId, int32_t type, const ::std::string& diskId,
    const ::std::string& partGuid, int32_t userId) {
  MutexAutoLock lock(sLock);
  DBG("%s, volId: %s, type: %d, diskId: %s, partGuid: %s\n", __func__,
      volId.c_str(), type, diskId.c_str(), partGuid.c_str());

  nsCString id(volId.c_str());
  nsCString diskid(diskId.c_str());
  nsCString partguid(partGuid.c_str());

  if (!id.Equals("emulated")) {
    DBG("Create volume: %s\n", volId.c_str());
    nsTArray<RefPtr<VolumeInfo>>& via = VolumeManager::GetVolumeInfoArray();
    via.AppendElement(new VolumeInfo(id, type, diskid, partguid));
  }
  VoldProxy::Mount(volId, VolumeInfo::kPrimary, 0);

  return android::binder::Status::ok();
}

// Because VoldListener doesn't inherit nsISupport interface, we cannot just
// use NewRunnableMethod to wrapp the function
void redirectVolumeStateChanged(const ::std::string& volId, int32_t state) {
  VoldListener::CreateInstance()->onVolumeStateChanged(volId, state);
}

android::binder::Status VoldListener::onVolumeStateChanged(
    const ::std::string& volId, int32_t state) {
  if (MessageLoop::current() != XRE_GetIOMessageLoop()) {
    XRE_GetIOMessageLoop()->PostTask(
        NewRunnableFunction("onVolumeStateChanged::iothread",
                            redirectVolumeStateChanged, volId, state));
    return android::binder::Status::ok();
  }

  MutexAutoLock lock(sLock);
  DBG("%s, volId: %s, state: %d\n", __func__, volId.c_str(), state);

  nsTArray<RefPtr<VolumeInfo>>& via = VolumeManager::GetVolumeInfoArray();
  nsCString id(volId.c_str());
  for (uint32_t volIndex = 0; volIndex < via.Length(); volIndex++) {
    if (id.Equals(via[volIndex]->getId())) {
      // setState of VolumeInfo
      via[volIndex]->setState(state);
      if (state == static_cast<int32_t>(VolumeInfo::State::kMounted)) {
        RefPtr<Volume> vol =
            VolumeManager::FindAddVolumeByName("sdcard1"_ns, id);
        // update state and set mountpoint of volume
        DBG("Add sdcard1 and update volume state!\n");
        vol->SetMountPoint(via[volIndex]->getMountPoint());
        vol->HandleVolumeStateChanged(state);
      } else {
        RefPtr<Volume> vol = VolumeManager::FindVolumeByName("sdcard1"_ns);
        if (!vol) {
          DBG("No volume name sdcard1 found!\n");
          break;
        }
        // update state of volume
        DBG("Update volume state of sdcard1!\n");
        vol->HandleVolumeStateChanged(state);
      }
      break;
    }
  }
  return android::binder::Status::ok();
}

android::binder::Status VoldListener::onVolumeMetadataChanged(
    const ::std::string& volId, const ::std::string& fsType,
    const ::std::string& fsUuid, const ::std::string& fsLabel) {
  MutexAutoLock lock(sLock);
  DBG("%s, volId: %s, fsType: %s, fsUuid: %s, fsLabel: %s\n", __func__,
      volId.c_str(), fsType.c_str(), fsUuid.c_str(), fsLabel.c_str());

  nsTArray<RefPtr<VolumeInfo>>& via = VolumeManager::GetVolumeInfoArray();
  nsCString id(volId.c_str());
  nsCString type(fsType.c_str());
  nsCString uuid(fsUuid.c_str());
  nsCString label(fsLabel.c_str());

  for (uint32_t volIndex = 0; volIndex < via.Length(); volIndex++) {
    if (id.Equals(via[volIndex]->getId())) {
      via[volIndex]->setFsType(type);
      via[volIndex]->setUuid(uuid);
      via[volIndex]->setFsLabel(label);
      break;
    }
  }
  return android::binder::Status::ok();
}

android::binder::Status VoldListener::onVolumePathChanged(
    const ::std::string& volId, const ::std::string& path) {
  MutexAutoLock lock(sLock);
  DBG("%s, volId: %s, path: %s\n", __func__, volId.c_str(), path.c_str());

  nsTArray<RefPtr<VolumeInfo>>& via = VolumeManager::GetVolumeInfoArray();
  nsCString id(volId.c_str());
  nsCString mountPoint(path.c_str());
  for (uint32_t volIndex = 0; volIndex < via.Length(); volIndex++) {
    if (id.Equals(via[volIndex]->getId())) {
      via[volIndex]->setMountPoint(mountPoint);
      break;
    }
  }
  return android::binder::Status::ok();
}

android::binder::Status VoldListener::onVolumeInternalPathChanged(
    const ::std::string& volId, const ::std::string& internalPath) {
  MutexAutoLock lock(sLock);
  DBG("%s, volId: %s, internalPath: %s\n", __func__, volId.c_str(),
      internalPath.c_str());

  nsTArray<RefPtr<VolumeInfo>>& via = VolumeManager::GetVolumeInfoArray();
  nsCString id(volId.c_str());
  nsCString internalMountPoint(internalPath.c_str());
  for (uint32_t volIndex = 0; volIndex < via.Length(); volIndex++) {
    if (id.Equals(via[volIndex]->getId())) {
      via[volIndex]->setInternalMountPoint(internalMountPoint);
      break;
    }
  }
  return android::binder::Status::ok();
}

android::binder::Status VoldListener::onVolumeDestroyed(
    const ::std::string& volId) {
  MutexAutoLock lock(sLock);
  DBG("%s, volId: %s\n", __func__, volId.c_str());

  nsTArray<RefPtr<VolumeInfo>>& via = VolumeManager::GetVolumeInfoArray();
  nsCString id(volId.c_str());
  for (uint32_t volIndex = 0; volIndex < via.Length(); volIndex++) {
    if (id.Equals(via[volIndex]->getId())) {
      via.RemoveElementAt(volIndex);
      break;
    }
  }
  return android::binder::Status::ok();
}

VoldProxy::~VoldProxy() {
  VoldListener::CleanUp();
  gVold = nullptr;
}

bool VoldProxy::Init() {
  sp<IServiceManager> sm = android::defaultServiceManager();
  sp<IBinder> binderVold;
  Status status;

  VolumeManager::SetState(VolumeManager::STARTING);
  binderVold = sm->getService(android::String16("vold"));
  gVold = android::interface_cast<android::os::IVold>(binderVold);

  status =
      gVold->setListener(android::interface_cast<android::os::IVoldListener>(
          VoldListener::CreateInstance()));
  if (!status.isOk()) {
    ERR("gVold->setListener failed!\n");
    return false;
  } else {
    LOG("Connect to vold!");
  }

  return true;
}

#define IMPL_VOLD_FUNCTION(_func, arg...) \
  do {                                    \
    if (!gVold) {                         \
      ERR("gVold is null\n");             \
      return false;                       \
    }                                     \
    Status status = gVold->_func(arg);    \
    if (!status.isOk()) {                 \
      ERR("Call %s failed!\n", __func__); \
      return false;                       \
    }                                     \
    return true;                          \
  } while (0)

bool VoldProxy::Reset() { IMPL_VOLD_FUNCTION(reset); }

bool VoldProxy::OnUserAdded(int32_t aUserId, int32_t aUserSerial) {
  IMPL_VOLD_FUNCTION(onUserAdded, aUserId, aUserSerial);
}

bool VoldProxy::OnUserStarted(int32_t aUserId) {
  IMPL_VOLD_FUNCTION(onUserStarted, aUserId);
}

bool VoldProxy::OnSecureKeyguardStateChanged(bool aEnabled) {
  IMPL_VOLD_FUNCTION(onSecureKeyguardStateChanged, aEnabled);
}

bool VoldProxy::Mount(const ::std::string& volId, int32_t mountFlag,
                      int32_t mountUserId) {
  IMPL_VOLD_FUNCTION(mount, volId, mountFlag, mountUserId, 0);
}

bool VoldProxy::Unmount(const ::std::string& volId) {
  IMPL_VOLD_FUNCTION(unmount, volId);
}

bool VoldProxy::Format(const ::std::string& volId,
                       const ::std::string& fsType) {
  IMPL_VOLD_FUNCTION(format, volId, fsType);
}

}  // end of namespace system
}  // namespace mozilla
