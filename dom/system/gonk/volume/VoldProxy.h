/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef VoldProxy_H
#define VoldProxy_H

#include <android/os/BnVoldListener.h>
#include <android/os/IVoldListener.h>
#include <binder/BinderService.h>

#include "VolumeManager.h"

using ::android::os::IVoldListener;

namespace mozilla {
namespace system {

class VolumeManager;
class VoldProxy;

class VoldListener : public android::BinderService<VoldListener>,
                     public android::os::BnVoldListener {
 public:
  VoldListener() : android::os::BnVoldListener() {}
  virtual ~VoldListener() = default;

  static VoldListener* CreateInstance();
  static void CleanUp();

  // IVoldListener.aidl
  android::binder::Status onDiskCreated(const ::std::string& diskId,
                                        int32_t flags) override;
  android::binder::Status onDiskScanned(const ::std::string& diskId) override;
  android::binder::Status onDiskMetadataChanged(
      const ::std::string& diskId, int64_t sizeBytes,
      const ::std::string& label, const ::std::string& sysPath) override;
  android::binder::Status onDiskDestroyed(const ::std::string& diskId) override;
  android::binder::Status onVolumeCreated(
      const ::std::string& volId, int32_t type, const ::std::string& diskId,
      const ::std::string& partGuid, int32_t userId = 0) override;
  android::binder::Status onVolumeStateChanged(const ::std::string& volId,
                                               int32_t state) override;
  android::binder::Status onVolumeMetadataChanged(
      const ::std::string& volId, const ::std::string& fsType,
      const ::std::string& fsUuid, const ::std::string& fsLabel) override;
  android::binder::Status onVolumePathChanged(
      const ::std::string& volId, const ::std::string& path) override;
  android::binder::Status onVolumeInternalPathChanged(
      const ::std::string& volId, const ::std::string& internalPath) override;
  android::binder::Status onVolumeDestroyed(
      const ::std::string& volId) override;

 private:
  static VoldListener* sInstance;
  static mozilla::Mutex sLock;
};

class VoldProxy final {
 public:
  VoldProxy() = default;

 private:
  friend class VoldListener;   // Calls Mount
  friend class Volume;         // Calls Mount, Unmount and Format.
  friend class VolumeManager;  // Calls Init, Reset, OnUserAdded, OnUserStarted
                               // and OnSecureKeyguardStateChanged.

  ~VoldProxy();

  static bool Init();
  static bool Reset();
  static bool OnUserAdded(int aUserId, int aUserSerial);
  static bool OnUserStarted(int aUserId);
  static bool OnSecureKeyguardStateChanged(bool aEnabled);
  static bool Mount(const ::std::string& volId, int32_t mountFlag,
                    int32_t mountUserId);
  static bool Unmount(const ::std::string& volId);
  static bool Format(const ::std::string& volId, const ::std::string& fsType);
};
}  // namespace system
}  // namespace mozilla

#endif  // VoldProxy_H
