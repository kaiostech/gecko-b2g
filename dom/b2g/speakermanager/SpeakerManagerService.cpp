/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SpeakerManagerService.h"

#include "AudioChannelService.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Unused.h"
#include "nsIAudioManager.h"
#include "nsIPropertyBag2.h"
#include "nsThreadUtils.h"
#include "nsServiceManagerUtils.h"
#include "SpeakerManagerServiceChild.h"

#include <cutils/properties.h>

#define NS_AUDIOMANAGER_CONTRACTID "@mozilla.org/telephony/audiomanager;1"

using namespace mozilla;
using namespace mozilla::dom;

StaticRefPtr<SpeakerManagerService> gSpeakerManagerService;

/* static */
SpeakerManagerService*
SpeakerManagerService::GetOrCreateSpeakerManagerService() {
  MOZ_ASSERT(NS_IsMainThread());

  if (!XRE_IsParentProcess()) {
    return SpeakerManagerServiceChild::GetOrCreateSpeakerManagerService();
  }

  // If we already exist, exit early
  if (gSpeakerManagerService) {
    return gSpeakerManagerService;
  }

  // Create new instance, register, return
  RefPtr<SpeakerManagerService> service = new SpeakerManagerService();

  gSpeakerManagerService = service;

  return gSpeakerManagerService;
}

SpeakerManagerService* SpeakerManagerService::GetSpeakerManagerService() {
  MOZ_ASSERT(NS_IsMainThread());

  if (!XRE_IsParentProcess()) {
    return SpeakerManagerServiceChild::GetSpeakerManagerService();
  }

  return gSpeakerManagerService;
}

/* static */
LogModule* SpeakerManagerService::GetSpeakerManagerLog() {
  static LazyLogModule gSpeakerManagerLog("SpeakerManager");
  return gSpeakerManagerLog;
}

void SpeakerManagerService::Shutdown() {
  // Clone the registered SpeakerManagers to call shutdown because
  // SpeakerManager::Shutdown touches mRegisteredSpeakerManagers.
  const auto managers = ToArray(mRegisteredSpeakerManagers.Values());
  for (auto sm : managers) {
    sm->Shutdown();
  }

  if (gSpeakerManagerService) {
    gSpeakerManagerService = nullptr;
  }
}

NS_IMPL_ISUPPORTS(SpeakerManagerService, nsIObserver)

void SpeakerManagerService::SpeakerManagerList::InsertData(
    const SpeakerManagerData& aData) {
  for (auto& data : *this) {
    // If the data of same child/window ID already exists, just update its
    // mForceSpeaker.
    if (data.mChildID == aData.mChildID && data.mWindowID == aData.mWindowID) {
      data.mForceSpeaker = aData.mForceSpeaker;
      return;
    }
  }
  // Not exist. Append to the end of the list.
  AppendElement(aData);
}

void SpeakerManagerService::SpeakerManagerList::RemoveData(
    const SpeakerManagerData& aData) {
  size_type i = 0;
  for (; i < Length(); ++i) {
    SpeakerManagerData& data = ElementAt(i);
    if (data.mChildID == aData.mChildID && data.mWindowID == aData.mWindowID) {
      break;
    }
  }
  if (i < Length()) {
    RemoveElementAt(i);
  }
}

void SpeakerManagerService::SpeakerManagerList::RemoveChild(uint64_t aChildID) {
  SpeakerManagerList temp;
  for (auto& data : *this) {
    if (data.mChildID != aChildID) {
      temp.AppendElement(data);
    }
  }
  SwapElements(temp);
}

void SpeakerManagerService::UpdateSpeakerStatus() {
  bool forceSpeaker = false;
  // Rule 1 - if foreground APP created SpeakerManager, always respect its
  //          forceSpeaker setting.
  if (!mVisibleSpeakerManagers.IsEmpty()) {
    forceSpeaker = mVisibleSpeakerManagers.LastElement().mForceSpeaker;
    // Rule 2 - if foreground APP did not create SpeakerManager, always respect
    //          the setting of the APP with active audio channel.
  } else if (!mActiveSpeakerManagers.IsEmpty()) {
    forceSpeaker = mActiveSpeakerManagers.LastElement().mForceSpeaker;
  }
  // Rule 3 - if rule 1 & 2 are not applied, disable forceSpeaker by default.

  if (mOrgSpeakerStatus != forceSpeaker) {
    mOrgSpeakerStatus = forceSpeaker;
    TurnOnSpeaker(forceSpeaker);
  }
}

void SpeakerManagerService::ForceSpeaker(bool aEnable, bool aVisible,
                                         bool aAudioChannelActive,
                                         uint64_t aWindowID,
                                         uint64_t aChildID) {
  MOZ_LOG(GetSpeakerManagerLog(), LogLevel::Debug,
          ("SpeakerManagerService, ForceSpeaker, enable %d, visible %d, active "
           "%d, child %lu, window %lu",
           aEnable, aVisible, aAudioChannelActive, aChildID, aWindowID));

  SpeakerManagerData data(aChildID, aWindowID, aEnable);

  // Update list of visible SpeakerManagers.
  if (aVisible) {
    mVisibleSpeakerManagers.InsertData(data);
  } else {
    mVisibleSpeakerManagers.RemoveData(data);
  }

  // Update list of SpeakerManagers with active audio channel.
  if (aAudioChannelActive) {
    mActiveSpeakerManagers.InsertData(data);
  } else {
    mActiveSpeakerManagers.RemoveData(data);
  }

  UpdateSpeakerStatus();
  Notify();
}

void SpeakerManagerService::TurnOnSpeaker(bool aOn) {
  nsCOMPtr<nsIAudioManager> audioManager =
      do_GetService(NS_AUDIOMANAGER_CONTRACTID);
  NS_ENSURE_TRUE_VOID(audioManager);
  int32_t phoneState;
  audioManager->GetPhoneState(&phoneState);
  int32_t forceuse =
      (phoneState == nsIAudioManager::PHONE_STATE_IN_CALL ||
       phoneState == nsIAudioManager::PHONE_STATE_IN_COMMUNICATION)
          ? nsIAudioManager::USE_COMMUNICATION
          : nsIAudioManager::USE_MEDIA;
  MOZ_LOG(GetSpeakerManagerLog(), LogLevel::Debug,
          ("SpeakerManagerService, TurnOnSpeaker, forceuse %d, enable %d",
           forceuse, aOn));
  if (aOn) {
    audioManager->SetForceForUse(forceuse, nsIAudioManager::FORCE_SPEAKER);
  } else {
    audioManager->SetForceForUse(forceuse, nsIAudioManager::FORCE_NONE);
  }
}

bool SpeakerManagerService::GetSpeakerStatus() {
  char propQemu[PROPERTY_VALUE_MAX];
  property_get("ro.kernel.qemu", propQemu, "");
  if (!strncmp(propQemu, "1", 1)) {
    return mOrgSpeakerStatus;
  }
  nsCOMPtr<nsIAudioManager> audioManager =
      do_GetService(NS_AUDIOMANAGER_CONTRACTID);
  NS_ENSURE_TRUE(audioManager, false);
  int32_t usage;
  audioManager->GetForceForUse(nsIAudioManager::USE_MEDIA, &usage);
  return usage == nsIAudioManager::FORCE_SPEAKER;
}

void SpeakerManagerService::Notify() {
  // Parent Notify to all the child processes.
  nsTArray<ContentParent*> children;
  ContentParent::GetAll(children);
  for (uint32_t i = 0; i < children.Length(); i++) {
    Unused << children[i]->SendSpeakerManagerNotify();
  }

  for (auto iter = mRegisteredSpeakerManagers.Iter(); !iter.Done();
       iter.Next()) {
    RefPtr<SpeakerManager> sm = iter.Data();
    sm->DispatchSimpleEvent(u"speakerforcedchange"_ns);
  }
}

void SpeakerManagerService::SetAudioChannelActive(bool aIsActive) {
  for (auto iter = mRegisteredSpeakerManagers.Iter(); !iter.Done();
       iter.Next()) {
    RefPtr<SpeakerManager> sm = iter.Data();
    sm->SetAudioChannelActive(aIsActive);
  }
}

NS_IMETHODIMP
SpeakerManagerService::Observe(nsISupports* aSubject, const char* aTopic,
                               const char16_t* aData) {
  if (!strcmp(aTopic, "ipc:content-shutdown")) {
    nsCOMPtr<nsIPropertyBag2> props = do_QueryInterface(aSubject);
    if (!props) {
      NS_WARNING(
          "ipc:content-shutdown message without property bag as subject");
      return NS_OK;
    }

    uint64_t childID = 0;
    nsresult rv = props->GetPropertyAsUint64(u"childID"_ns, &childID);
    if (NS_SUCCEEDED(rv)) {
      MOZ_LOG(GetSpeakerManagerLog(), LogLevel::Debug,
              ("SpeakerManagerService, Observe, remove child %lu", childID));
      mVisibleSpeakerManagers.RemoveChild(childID);
      mActiveSpeakerManagers.RemoveChild(childID);
      UpdateSpeakerStatus();
    } else {
      NS_WARNING("ipc:content-shutdown message without childID property");
    }
  } else if (!strcmp(aTopic, "xpcom-will-shutdown")) {
    // Note that we need to do this before xpcom-shutdown, since the
    // AudioChannelService cannot be used past that point.
    RefPtr<AudioChannelService> audioChannelService =
        AudioChannelService::GetOrCreate();
    if (audioChannelService) {
      audioChannelService->UnregisterSpeakerManager(this);
    }

    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
      if (XRE_IsParentProcess()) {
        obs->RemoveObserver(this, "ipc:content-shutdown");
      }
      obs->RemoveObserver(this, "xpcom-will-shutdown");
    }

    Shutdown();
  }
  return NS_OK;
}

SpeakerManagerService::SpeakerManagerService() : mOrgSpeakerStatus(false) {
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    if (XRE_IsParentProcess()) {
      obs->AddObserver(this, "ipc:content-shutdown", false);
    }
    obs->AddObserver(this, "xpcom-will-shutdown", false);
  }

  RefPtr<AudioChannelService> audioChannelService =
      AudioChannelService::GetOrCreate();
  if (audioChannelService) {
    audioChannelService->RegisterSpeakerManager(this);
  }
}
