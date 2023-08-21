/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VolumeManager.h"

#include "Volume.h"
#include "VolumeManagerLog.h"
#include "VolumeServiceTest.h"

#include "nsWhitespaceTokenizer.h"
#include "nsXULAppAPI.h"

#include "base/message_loop.h"
#include "base/task.h"
#include "mozilla/Scoped.h"
#include "mozilla/StaticPtr.h"

#include <android/log.h>
#include <cutils/sockets.h>
#include <fcntl.h>
#include <sys/socket.h>

namespace mozilla {
namespace system {

static StaticRefPtr<VolumeManager> sVolumeManager;

VolumeManager::STATE VolumeManager::mState = VolumeManager::UNINITIALIZED;
VolumeManager::StateObserverList VolumeManager::mStateObserverList;

VolumeInfo::VolumeInfo(const nsACString& aId, int aType,
                       const nsACString& aDiskId, const nsACString& aPartGuid)
    : mId(aId),
      mType(aType),
      mDiskId(aDiskId),
      mPartGuid(aPartGuid),
      mState(State::kUnmounted) {
  LOG("create VolumeInfo Id=%s, diskId=%s state = %d, type = %d", mId.Data(),
      mDiskId.Data(), mState, mType);
}

VolumeManager::VolumeManager() { DBG("VolumeManager constructor called"); }

VolumeManager::~VolumeManager() {}

// static
void VolumeManager::Dump(const char* aLabel) {
  if (!sVolumeManager) {
    LOG("%s: sVolumeManager == null", aLabel);
    return;
  }

  VolumeArray::size_type numVolumes = NumVolumes();
  VolumeArray::index_type volIndex;
  for (volIndex = 0; volIndex < numVolumes; volIndex++) {
    RefPtr<Volume> vol = GetVolume(volIndex);
    vol->Dump(aLabel);
  }
}

// static
size_t VolumeManager::NumVolumes() {
  if (!sVolumeManager) {
    return 0;
  }
  return sVolumeManager->mVolumeArray.Length();
}

// static
already_AddRefed<Volume> VolumeManager::GetVolume(size_t aIndex) {
  MOZ_ASSERT(aIndex < NumVolumes());
  RefPtr<Volume> vol = sVolumeManager->mVolumeArray[aIndex];
  return vol.forget();
}

// static
VolumeManager::STATE VolumeManager::State() { return mState; }

// static
const char* VolumeManager::StateStr(VolumeManager::STATE aState) {
  switch (aState) {
    case UNINITIALIZED:
      return "Uninitialized";
    case STARTING:
      return "Starting";
    case VOLUMES_READY:
      return "Volumes Ready";
  }
  return "???";
}

// static
void VolumeManager::SetState(STATE aNewState) {
  if (mState != aNewState) {
    LOG("changing state from '%s' to '%s'", StateStr(mState),
        StateStr(aNewState));
    mState = aNewState;
    mStateObserverList.Broadcast(StateChangedEvent());
  }
}

// static
void VolumeManager::RegisterStateObserver(StateObserver* aObserver) {
  mStateObserverList.AddObserver(aObserver);
}

// static
void VolumeManager::UnregisterStateObserver(StateObserver* aObserver) {
  mStateObserverList.RemoveObserver(aObserver);
}

// static
already_AddRefed<Volume> VolumeManager::FindVolumeByName(
    const nsACString& aName) {
  if (!sVolumeManager) {
    return nullptr;
  }
  VolumeArray::size_type numVolumes = NumVolumes();
  VolumeArray::index_type volIndex;
  for (volIndex = 0; volIndex < numVolumes; volIndex++) {
    RefPtr<Volume> vol = GetVolume(volIndex);
    if (vol->Name().Equals(aName)) {
      return vol.forget();
    }
  }
  return nullptr;
}

// static
already_AddRefed<Volume> VolumeManager::FindAddVolumeByName(
    const nsACString& aName) {
  RefPtr<Volume> vol = FindVolumeByName(aName);
  if (vol) {
    return vol.forget();
  }
  // No volume found, create and add a new one.
  vol = new Volume(aName);
  sVolumeManager->mVolumeArray.AppendElement(vol);
  return vol.forget();
}
// static
already_AddRefed<Volume> VolumeManager::FindAddVolumeByName(
    const nsACString& aName, const nsACString& aUuid) {
  RefPtr<Volume> vol = FindVolumeByName(aName);
  if (vol) {
    return vol.forget();
  }
  // No volume found, create and add a new one.
  vol = new Volume(aName, aUuid);
  sVolumeManager->mVolumeArray.AppendElement(vol);
  return vol.forget();
}

// static
bool VolumeManager::RemoveVolumeByName(const nsACString& aName) {
  if (!sVolumeManager) {
    return false;
  }
  VolumeArray::size_type numVolumes = NumVolumes();
  VolumeArray::index_type volIndex;
  for (volIndex = 0; volIndex < numVolumes; volIndex++) {
    RefPtr<Volume> vol = GetVolume(volIndex);
    if (vol->Name().Equals(aName)) {
      sVolumeManager->mVolumeArray.RemoveElementAt(volIndex);
      return true;
    }
  }
  // No volume found. Return false to indicate this.
  return false;
}

// static
void VolumeManager::InitConfig() {
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());

  // This function uses /system/etc/volume.cfg to add additional volumes
  // to the Volume Manager.
  //
  // This is useful on devices like the Nexus 4, which have no physical sd card
  // or dedicated partition.
  //
  // The format of the volume.cfg file is as follows:
  // create volume-name mount-point
  // configure volume-name preference preference-value
  // Blank lines and lines starting with the hash character "#" will be ignored.

  ScopedCloseFile fp;
  int n = 0;
  char line[255];
  const char* filename = "/system/etc/volume.cfg";
  if (!(fp = fopen(filename, "r"))) {
    LOG("Unable to open volume configuration file '%s' - ignoring", filename);
    return;
  }
  while (fgets(line, sizeof(line), fp)) {
    n++;

    if (line[0] == '#') continue;

    nsCString commandline(line);
    nsCWhitespaceTokenizer tokenizer(commandline);
    if (!tokenizer.hasMoreTokens()) {
      // Blank line - ignore
      continue;
    }

    nsCString command(tokenizer.nextToken());
    if (command.EqualsLiteral("create")) {
      if (!tokenizer.hasMoreTokens()) {
        ERR("No vol_name in %s line %d", filename, n);
        continue;
      }
      nsCString volName(tokenizer.nextToken());
      if (!tokenizer.hasMoreTokens()) {
        ERR("No mount point for volume '%s'. %s line %d", volName.get(),
            filename, n);
        continue;
      }
      nsCString mountPoint(tokenizer.nextToken());
      RefPtr<Volume> vol = FindAddVolumeByName(volName);
      vol->SetFakeVolume(mountPoint);
      continue;
    }
    if (command.EqualsLiteral("configure")) {
      if (!tokenizer.hasMoreTokens()) {
        ERR("No vol_name in %s line %d", filename, n);
        continue;
      }
      nsCString volName(tokenizer.nextToken());
      if (!tokenizer.hasMoreTokens()) {
        ERR("No configuration name specified for volume '%s'. %s line %d",
            volName.get(), filename, n);
        continue;
      }
      nsCString configName(tokenizer.nextToken());
      if (!tokenizer.hasMoreTokens()) {
        ERR("No value for configuration name '%s'. %s line %d",
            configName.get(), filename, n);
        continue;
      }
      nsCString configValue(tokenizer.nextToken());
      RefPtr<Volume> vol = FindVolumeByName(volName);
      if (vol) {
        vol->SetConfig(configName, configValue);
      } else {
        ERR("Invalid volume name '%s'.", volName.get());
      }
      continue;
    }
    if (command.EqualsLiteral("ignore")) {
      // This command is useful to remove volumes which are being tracked by
      // vold, but for which we have no interest.
      if (!tokenizer.hasMoreTokens()) {
        ERR("No vol_name in %s line %d", filename, n);
        continue;
      }
      nsCString volName(tokenizer.nextToken());
      RemoveVolumeByName(volName);
      continue;
    }
    ERR("Unrecognized command: '%s'", command.get());
  }
}

void VolumeManager::DefaultConfig() {
  VolumeManager::VolumeArray::size_type numVolumes =
      VolumeManager::NumVolumes();
  if (numVolumes == 0) {
    return;
  }
  if (numVolumes == 1) {
    // This is to cover early shipping phones like the Buri,
    // which had no internal storage, and only external sdcard.
    //
    // Phones line the nexus-4 which only have an internal
    // storage area will need to have a volume.cfg file with
    // removable set to false.
    RefPtr<Volume> vol = VolumeManager::GetVolume(0);
    vol->SetIsRemovable(true);
    vol->SetIsHotSwappable(true);
    return;
  }
  VolumeManager::VolumeArray::index_type volIndex;
  for (volIndex = 0; volIndex < numVolumes; volIndex++) {
    RefPtr<Volume> vol = VolumeManager::GetVolume(volIndex);
    if (!vol->Name().EqualsLiteral("sdcard")) {
      vol->SetIsRemovable(true);
      vol->SetIsHotSwappable(true);
    }
  }
}

bool VolumeManager::InitVold() {
  SetState(STARTING);

  if (!VoldProxy::Init()) {
    return false;
  }

  if (!VoldProxy::Reset()) {
    return false;
  }

  DefaultConfig();
  InitConfig();
  Dump("READY");
  SetState(VolumeManager::VOLUMES_READY);

  if (!VoldProxy::OnUserAdded(0, 0) || !VoldProxy::OnUserStarted(0) ||
      !VoldProxy::OnSecureKeyguardStateChanged(false)) {
    return false;
  }

  return true;
}

nsTArray<RefPtr<VolumeInfo>>& VolumeManager::GetVolumeInfoArray() {
  return sVolumeManager->mVolumeInfoArray;
}

void VolumeManager::Restart() { Start(); }

// static
void VolumeManager::Start() {
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());

  if (!sVolumeManager) {
    return;
  }
  SetState(STARTING);
  if (!sVolumeManager->InitVold()) {
    // Initialize vold failed, try again in a second.
    MessageLoopForIO::current()->PostDelayedTask(
        NewRunnableFunction("VolumeManagerStartRunnable", VolumeManager::Start),
        1000);
  }
}

void VolumeManager::OnError() { Restart(); }

/***************************************************************************/

static void InitVolumeManagerIOThread() {
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());
  MOZ_ASSERT(!sVolumeManager);

  sVolumeManager = new VolumeManager();
  VolumeManager::Start();

  InitVolumeServiceTestIOThread();
}

static void ShutdownVolumeManagerIOThread() {
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());

  sVolumeManager = nullptr;
}

/**************************************************************************
 *
 *   Public API
 *
 *   Since the VolumeManager runs in IO Thread context, we need to switch
 *   to IOThread context before we can do anything.
 *
 **************************************************************************/

void InitVolumeManager() {
  XRE_GetIOMessageLoop()->PostTask(NewRunnableFunction(
      "InitVolumeManagerIOThreadRunnable", InitVolumeManagerIOThread));
}

void ShutdownVolumeManager() {
  ShutdownVolumeServiceTest();

  XRE_GetIOMessageLoop()->PostTask(NewRunnableFunction(
      "ShutdownVolumeManagerIOThreadRunnable", ShutdownVolumeManagerIOThread));
}

}  // namespace system
}  // namespace mozilla
