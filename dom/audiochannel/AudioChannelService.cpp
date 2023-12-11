/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioChannelService.h"

#include "base/basictypes.h"

#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/Document.h"

#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/BrowserParent.h"

#include "nsContentUtils.h"
#include "nsIObserverService.h"
#include "nsISupportsPrimitives.h"
#include "nsThreadUtils.h"
#include "nsHashPropertyBag.h"
#include "nsComponentManagerUtils.h"
#include "nsPIDOMWindow.h"
#include "nsServiceManagerUtils.h"

#include "mozilla/Preferences.h"

#ifdef MOZ_WIDGET_GONK
#  include "SpeakerManagerService.h"
#endif

using namespace mozilla;
using namespace mozilla::dom;

mozilla::LazyLogModule gAudioChannelLog("AudioChannel");

namespace {

bool sXPCOMShuttingDown = false;

class NotifyChannelActiveRunnable final : public Runnable {
 public:
  NotifyChannelActiveRunnable(uint64_t aWindowID, AudioChannel aAudioChannel,
                              bool aActive)
      : Runnable("NotifyChannelActiveRunnable"),
        mWindowID(aWindowID),
        mAudioChannel(aAudioChannel),
        mActive(aActive) {}

  NS_IMETHOD Run() override {
    nsCOMPtr<nsIObserverService> observerService =
        services::GetObserverService();
    if (NS_WARN_IF(!observerService)) {
      return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsISupportsPRUint64> wrapper =
        do_CreateInstance(NS_SUPPORTS_PRUINT64_CONTRACTID);
    if (NS_WARN_IF(!wrapper)) {
      return NS_ERROR_FAILURE;
    }

    wrapper->SetData(mWindowID);

    nsAutoString name;
    AudioChannelService::GetAudioChannelString(mAudioChannel, name);

    nsAutoCString topic;
    topic.Assign("audiochannel-activity-");
    topic.Append(NS_ConvertUTF16toUTF8(name));

    observerService->NotifyObservers(wrapper, topic.get(),
                                     mActive ? u"active" : u"inactive");

    MOZ_LOG(AudioChannelService::GetAudioChannelLog(), LogLevel::Debug,
            ("NotifyChannelActiveRunnable, type = %" PRIu32 ", active = %s\n",
             static_cast<uint32_t>(mAudioChannel), mActive ? "true" : "false"));

    return NS_OK;
  }

 private:
  const uint64_t mWindowID;
  const AudioChannel mAudioChannel;
  const bool mActive;
};

class AudioPlaybackRunnable final : public Runnable {
 public:
  AudioPlaybackRunnable(nsPIDOMWindowOuter* aWindow, bool aActive,
                        AudioChannelService::AudibleChangedReasons aReason)
      : mozilla::Runnable("AudioPlaybackRunnable"),
        mWindow(aWindow),
        mActive(aActive),
        mReason(aReason) {}

  NS_IMETHOD Run() override {
    nsCOMPtr<nsIObserverService> observerService =
        services::GetObserverService();
    if (NS_WARN_IF(!observerService)) {
      return NS_ERROR_FAILURE;
    }

    nsAutoString state;
    GetActiveState(state);

    observerService->NotifyObservers(ToSupports(mWindow), "audio-playback",
                                     state.get());

    MOZ_LOG(AudioChannelService::GetAudioChannelLog(), LogLevel::Debug,
            ("AudioPlaybackRunnable, active = %s, reason = %s\n",
             mActive ? "true" : "false", AudibleChangedReasonToStr(mReason)));

    return NS_OK;
  }

 private:
  void GetActiveState(nsAString& aState) {
    if (mActive) {
      aState.AssignLiteral("active");
    } else {
      if (mReason ==
          AudioChannelService::AudibleChangedReasons::ePauseStateChanged) {
        aState.AssignLiteral("inactive-pause");
      } else {
        aState.AssignLiteral("inactive-nonaudible");
      }
    }
  }

  nsCOMPtr<nsPIDOMWindowOuter> mWindow;
  bool mActive;
  AudioChannelService::AudibleChangedReasons mReason;
};

bool IsChromeWindow(nsPIDOMWindowOuter* aWindow) {
  nsCOMPtr<nsPIDOMWindowInner> inner = aWindow->GetCurrentInnerWindow();
  if (!inner) {
    return false;
  }

  nsCOMPtr<Document> doc = inner->GetExtantDoc();
  if (!doc) {
    return false;
  }

  return nsContentUtils::IsChromeDoc(doc);
}

}  // anonymous namespace

namespace mozilla::dom {

const char* SuspendTypeToStr(const nsSuspendedTypes& aSuspend) {
  MOZ_ASSERT(aSuspend == nsISuspendedTypes::NONE_SUSPENDED ||
             aSuspend == nsISuspendedTypes::SUSPENDED_PAUSE ||
             aSuspend == nsISuspendedTypes::SUSPENDED_BLOCK);

  switch (aSuspend) {
    case nsISuspendedTypes::NONE_SUSPENDED:
      return "none";
    case nsISuspendedTypes::SUSPENDED_PAUSE:
      return "pause";
    case nsISuspendedTypes::SUSPENDED_BLOCK:
      return "block";
    default:
      return "unknown";
  }
}

const char* AudibleStateToStr(
    const AudioChannelService::AudibleState& aAudible) {
  MOZ_ASSERT(aAudible == AudioChannelService::AudibleState::eNotAudible ||
             aAudible == AudioChannelService::AudibleState::eMaybeAudible ||
             aAudible == AudioChannelService::AudibleState::eAudible);

  switch (aAudible) {
    case AudioChannelService::AudibleState::eNotAudible:
      return "not-audible";
    case AudioChannelService::AudibleState::eMaybeAudible:
      return "maybe-audible";
    case AudioChannelService::AudibleState::eAudible:
      return "audible";
    default:
      return "unknown";
  }
}

const char* AudibleChangedReasonToStr(
    const AudioChannelService::AudibleChangedReasons& aReason) {
  MOZ_ASSERT(
      aReason == AudioChannelService::AudibleChangedReasons::eVolumeChanged ||
      aReason ==
          AudioChannelService::AudibleChangedReasons::eDataAudibleChanged ||
      aReason ==
          AudioChannelService::AudibleChangedReasons::ePauseStateChanged);

  switch (aReason) {
    case AudioChannelService::AudibleChangedReasons::eVolumeChanged:
      return "volume";
    case AudioChannelService::AudibleChangedReasons::eDataAudibleChanged:
      return "data-audible";
    case AudioChannelService::AudibleChangedReasons::ePauseStateChanged:
      return "pause-state";
    default:
      return "unknown";
  }
}

StaticRefPtr<AudioChannelService> gAudioChannelService;

// Mappings from 'mozaudiochannel' attribute strings to an enumeration.
static const nsAttrValue::EnumTable kMozAudioChannelAttributeTable[] = {
    {"normal", (int16_t)AudioChannel::Normal},
    {"content", (int16_t)AudioChannel::Content},
    {"notification", (int16_t)AudioChannel::Notification},
    {"alarm", (int16_t)AudioChannel::Alarm},
    {"telephony", (int16_t)AudioChannel::Telephony},
    {"ringer", (int16_t)AudioChannel::Ringer},
    {"publicnotification", (int16_t)AudioChannel::Publicnotification},
    {"system", (int16_t)AudioChannel::System},
    {nullptr, 0}};

/* static */
void AudioChannelService::CreateServiceIfNeeded() {
  MOZ_ASSERT(NS_IsMainThread());

  if (!gAudioChannelService) {
    gAudioChannelService = new AudioChannelService();
  }
}

/* static */
already_AddRefed<AudioChannelService> AudioChannelService::GetOrCreate() {
  if (sXPCOMShuttingDown) {
    return nullptr;
  }

  CreateServiceIfNeeded();
  RefPtr<AudioChannelService> service = gAudioChannelService.get();
  return service.forget();
}

/* static */
already_AddRefed<AudioChannelService> AudioChannelService::Get() {
  if (sXPCOMShuttingDown) {
    return nullptr;
  }

  RefPtr<AudioChannelService> service = gAudioChannelService.get();
  return service.forget();
}

/* static */
LogModule* AudioChannelService::GetAudioChannelLog() {
  return gAudioChannelLog;
}

/* static */
void AudioChannelService::Shutdown() {
  if (gAudioChannelService) {
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
      obs->RemoveObserver(gAudioChannelService, "xpcom-shutdown");
      obs->RemoveObserver(gAudioChannelService, "outer-window-destroyed");
    }

    gAudioChannelService->mWindows.Clear();
    gAudioChannelService->mPlayingChildren.Clear();
    gAudioChannelService->mBrowserParents.Clear();
#ifdef MOZ_WIDGET_GONK
    gAudioChannelService->mSpeakerManagerServices.Clear();
#endif

    gAudioChannelService = nullptr;
  }
}

/* static */
already_AddRefed<nsPIDOMWindowOuter> AudioChannelService::GetTopAppWindow(
    nsPIDOMWindowOuter* aWindow) {
  if (!aWindow) {
    return nullptr;
  }
  return aWindow->GetInProcessTop();
}

/* static */
already_AddRefed<nsPIDOMWindowOuter> AudioChannelService::GetTopAppWindow(
    BrowserParent* aBrowserParent) {
  MOZ_ASSERT(aBrowserParent);

  nsCOMPtr<nsPIDOMWindowOuter> parent = aBrowserParent->GetParentWindowOuter();
  if (!parent) {
    MOZ_LOG(AudioChannelService::GetAudioChannelLog(), LogLevel::Warning,
            ("AudioChannelService, GetTopAppWindow, BrowserParent %p has no "
             "parent window. Is it managed by system app?\n",
             aBrowserParent));
    return nullptr;
  }

  // If this BrowserParent is owned by chrome doc, return nullptr.
  if (IsChromeWindow(parent)) {
    return nullptr;
  }

  return GetTopAppWindow(parent);
}

NS_INTERFACE_MAP_BEGIN(AudioChannelService)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(AudioChannelService)
NS_IMPL_RELEASE(AudioChannelService)

AudioChannelService::AudioChannelService()
    : mDefChannelChildID(hal::CONTENT_PROCESS_ID_UNKNOWN),
      mTelephonyChannel(false),
      mContentOrNormalChannel(false),
      mAnyChannel(false) {
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->AddObserver(this, "xpcom-shutdown", false);
    obs->AddObserver(this, "outer-window-destroyed", false);
    if (XRE_IsParentProcess()) {
      obs->AddObserver(this, "ipc:content-shutdown", false);
    }
  }
}

AudioChannelService::~AudioChannelService() = default;

void AudioChannelService::RegisterAudioChannelAgent(AudioChannelAgent* aAgent,
                                                    AudibleState aAudible) {
  MOZ_ASSERT(aAgent);

  uint64_t windowID = aAgent->WindowID();
  AudioChannelWindow* winData = GetWindowData(windowID);
  if (!winData) {
    winData = new AudioChannelWindow(windowID);
    mWindows.AppendElement(WrapUnique(winData));
  }

  // To make sure agent would be alive because AppendAgent() would trigger the
  // callback function of AudioChannelAgentOwner that means the agent might be
  // released in their callback.
  RefPtr<AudioChannelAgent> kungFuDeathGrip(aAgent);
  winData->AppendAgent(aAgent, aAudible);

  MaybeSendStatusUpdate();
}

void AudioChannelService::UnregisterAudioChannelAgent(
    AudioChannelAgent* aAgent) {
  MOZ_ASSERT(aAgent);

  AudioChannelWindow* winData = GetWindowData(aAgent->WindowID());
  if (!winData) {
    return;
  }

  // To make sure agent would be alive because AppendAgent() would trigger the
  // callback function of AudioChannelAgentOwner that means the agent might be
  // released in their callback.
  RefPtr<AudioChannelAgent> kungFuDeathGrip(aAgent);
  winData->RemoveAgent(aAgent);

#ifdef MOZ_WIDGET_GONK
  bool active = AnyAudioChannelIsActive(false);
  for (auto* service : mSpeakerManagerServices) {
    service->SetAudioChannelActive(active);
  }
#endif

  MaybeSendStatusUpdate();
}

void AudioChannelService::RegisterBrowserParent(BrowserParent* aBrowserParent) {
  MOZ_ASSERT(aBrowserParent);
  MOZ_ASSERT(!mBrowserParents.Contains(aBrowserParent));
  mBrowserParents.AppendElement(aBrowserParent);
}

void AudioChannelService::UnregisterBrowserParent(
    BrowserParent* aBrowserParent) {
  MOZ_ASSERT(aBrowserParent);
  mBrowserParents.RemoveElement(aBrowserParent);
}

AudioPlaybackConfig AudioChannelService::GetMediaConfig(
    nsPIDOMWindowOuter* aWindow, uint32_t aAudioChannel) const {
  MOZ_ASSERT(aAudioChannel < NUMBER_OF_AUDIO_CHANNELS);

#ifdef MOZ_WIDGET_GONK
  AudioPlaybackConfig config(1.0, false, InitialSuspendType());
  nsCOMPtr<nsPIDOMWindowOuter> window = GetTopAppWindow(aWindow);
  if (window) {
    AudioChannelWindow* winData = GetWindowData(window->WindowID());
    if (winData) {
      config.mVolume = winData->mChannels[aAudioChannel].mVolume;
      config.mMuted = winData->mChannels[aAudioChannel].mMuted;
      config.mSuspend = winData->mChannels[aAudioChannel].mSuspend;
    }
  }
  return config;
#else
  AudioPlaybackConfig config(1.0, false, nsISuspendedTypes::NONE_SUSPENDED);

  if (!aWindow) {
    config.mVolume = 0.0;
    config.mMuted = true;
    config.mSuspend = nsISuspendedTypes::SUSPENDED_BLOCK;
    return config;
  }

  AudioChannelWindow* winData = nullptr;
  nsCOMPtr<nsPIDOMWindowOuter> window = aWindow;

  // The volume must be calculated based on the window hierarchy. Here we go up
  // to the top window and we calculate the volume and the muted flag.
  do {
    winData = GetWindowData(window->WindowID());
    if (winData) {
      config.mVolume *= winData->mChannels[aAudioChannel].mVolume;
      config.mMuted = config.mMuted || winData->mChannels[aAudioChannel].mMuted;
      config.mCapturedAudio = winData->mIsAudioCaptured;
    }

    config.mMuted = config.mMuted || window->GetAudioMuted();
    if (window->ShouldDelayMediaFromStart()) {
      config.mSuspend = nsISuspendedTypes::SUSPENDED_BLOCK;
    }

    nsCOMPtr<nsPIDOMWindowOuter> win =
        window->GetInProcessScriptableParentOrNull();
    if (!win) {
      break;
    }

    window = win;

    // If there is no parent, or we are the toplevel we don't continue.
  } while (window && window != aWindow);

  return config;
#endif
}

void AudioChannelService::AudioAudibleChanged(AudioChannelAgent* aAgent,
                                              AudibleState aAudible,
                                              AudibleChangedReasons aReason) {
  MOZ_ASSERT(aAgent);

  uint64_t windowID = aAgent->WindowID();
  AudioChannelWindow* winData = GetWindowData(windowID);
  if (winData) {
    winData->AudioAudibleChanged(aAgent, aAudible, aReason);
  }
}

bool AudioChannelService::TelephonyChannelIsActive() {
  nsTObserverArray<UniquePtr<AudioChannelWindow>>::ForwardIterator windowsIter(
      mWindows);
  while (windowsIter.HasMore()) {
    auto& next = windowsIter.GetNext();
    if (next->IsChannelActive(AudioChannel::Telephony)) {
      return true;
    }
  }

  if (XRE_IsParentProcess()) {
    nsTObserverArray<UniquePtr<AudioChannelChildStatus>>::ForwardIterator
        childrenIter(mPlayingChildren);
    while (childrenIter.HasMore()) {
      auto& child = childrenIter.GetNext();
      if (child->mActiveTelephonyChannel) {
        return true;
      }
    }
  }

  return false;
}

bool AudioChannelService::ContentOrNormalChannelIsActive() {
  // This method is meant to be used just by the child to send status update.
  MOZ_ASSERT(!XRE_IsParentProcess());

  nsTObserverArray<UniquePtr<AudioChannelWindow>>::ForwardIterator iter(
      mWindows);
  while (iter.HasMore()) {
    auto& next = iter.GetNext();
    if (next->IsChannelActive(AudioChannel::Content) ||
        next->IsChannelActive(AudioChannel::Normal)) {
      return true;
    }
  }
  return false;
}

AudioChannelService::AudioChannelChildStatus*
AudioChannelService::GetChildStatus(uint64_t aChildID) const {
  nsTObserverArray<UniquePtr<AudioChannelChildStatus>>::ForwardIterator iter(
      mPlayingChildren);
  while (iter.HasMore()) {
    auto& child = iter.GetNext();
    if (child->mChildID == aChildID) {
      return child.get();
    }
  }

  return nullptr;
}

void AudioChannelService::RemoveChildStatus(uint64_t aChildID) {
  nsTObserverArray<UniquePtr<AudioChannelChildStatus>>::ForwardIterator iter(
      mPlayingChildren);
  while (iter.HasMore()) {
    UniquePtr<AudioChannelChildStatus>& child = iter.GetNext();
    if (child->mChildID == aChildID) {
      mPlayingChildren.RemoveElement(child);
      break;
    }
  }
}

bool AudioChannelService::ProcessContentOrNormalChannelIsActive(
    uint64_t aChildID) {
  AudioChannelChildStatus* child = GetChildStatus(aChildID);
  if (!child) {
    return false;
  }

  return child->mActiveContentOrNormalChannel;
}

bool AudioChannelService::AnyAudioChannelIsActive(bool aCheckChild) {
  nsTObserverArray<UniquePtr<AudioChannelWindow>>::ForwardIterator iter(
      mWindows);
  while (iter.HasMore()) {
    auto& next = iter.GetNext();
    for (uint32_t i = 0; kMozAudioChannelAttributeTable[i].tag; ++i) {
      AudioChannel channel =
          static_cast<AudioChannel>(kMozAudioChannelAttributeTable[i].value);
      if (next->IsChannelActive(channel)) {
        return true;
      }
    }
  }

  if (XRE_IsParentProcess() && aCheckChild) {
    return !mPlayingChildren.IsEmpty();
  }

  return false;
}

NS_IMETHODIMP
AudioChannelService::Observe(nsISupports* aSubject, const char* aTopic,
                             const char16_t* aData) {
  if (!strcmp(aTopic, "xpcom-shutdown")) {
    sXPCOMShuttingDown = true;
    Shutdown();
  } else if (!strcmp(aTopic, "outer-window-destroyed")) {
    nsCOMPtr<nsISupportsPRUint64> wrapper = do_QueryInterface(aSubject);
    NS_ENSURE_TRUE(wrapper, NS_ERROR_FAILURE);

    uint64_t outerID;
    nsresult rv = wrapper->GetData(&outerID);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    UniquePtr<AudioChannelWindow> winData;
    {
      nsTObserverArray<UniquePtr<AudioChannelWindow>>::ForwardIterator iter(
          mWindows);
      while (iter.HasMore()) {
        auto& next = iter.GetNext();
        if (next->mWindowID == outerID) {
          winData = std::move(next);
          iter.Remove();
          break;
        }
      }
    }

    if (winData) {
      for (AudioChannelAgent* agent : winData->mAgents.ForwardRange()) {
        int32_t channel = agent->AudioChannelType();
        agent->WindowVolumeChanged(winData->mChannels[channel].mVolume,
                                   winData->mChannels[channel].mMuted);
      }
    }

#ifdef MOZ_WIDGET_GONK
    bool active = AnyAudioChannelIsActive(false);
    for (auto* service : mSpeakerManagerServices) {
      service->SetAudioChannelActive(active);
    }
#endif
  } else if (!strcmp(aTopic, "ipc:content-shutdown")) {
    nsCOMPtr<nsIPropertyBag2> props = do_QueryInterface(aSubject);
    if (!props) {
      NS_WARNING(
          "ipc:content-shutdown message without property bag as subject");
      return NS_OK;
    }

    uint64_t childID = 0;
    nsresult rv = props->GetPropertyAsUint64(u"childID"_ns, &childID);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (mDefChannelChildID == childID) {
      SetDefaultVolumeControlChannelInternal(-1, false, childID);
      mDefChannelChildID = hal::CONTENT_PROCESS_ID_UNKNOWN;
    }

    RemoveChildStatus(childID);
  }

  return NS_OK;
}

void AudioChannelService::RefreshAgents(
    nsPIDOMWindowOuter* aWindow,
    const std::function<void(AudioChannelAgent*)>& aFunc) {
  MOZ_ASSERT(aWindow);

  nsCOMPtr<nsPIDOMWindowOuter> topWindow = aWindow->GetInProcessScriptableTop();
  if (!topWindow) {
    return;
  }

  AudioChannelWindow* winData = GetWindowData(topWindow->WindowID());
  if (!winData) {
    return;
  }

  for (AudioChannelAgent* agent : winData->mAgents.ForwardRange()) {
    aFunc(agent);
  }
}

void AudioChannelService::RefreshAgentsVolume(nsPIDOMWindowOuter* aWindow,
                                              AudioChannel aChannel,
                                              float aVolume, bool aMuted) {
  auto func = [aChannel, aVolume, aMuted](AudioChannelAgent* agent) {
    if (agent->AudioChannelType() == static_cast<int32_t>(aChannel)) {
      agent->WindowVolumeChanged(aVolume, aMuted);
    }
  };
  RefreshAgents(aWindow, func);
}

void AudioChannelService::RefreshAgentsSuspend(nsPIDOMWindowOuter* aWindow,
                                               AudioChannel aChannel,
                                               nsSuspendedTypes aSuspend) {
#ifdef MOZ_WIDGET_GONK
  // If our audio channel activity is switched from inactive to active,
  // we need to refresh SpeakerManagers before AudioChannelAgents are
  // unmuted. Otherwise it may leaves a gap of the sound coming from
  // inccorrect output device.
  bool active = AnyAudioChannelIsActive(false);
  if (active) {
    for (auto* service : mSpeakerManagerServices) {
      service->SetAudioChannelActive(active);
    }
  }
#endif

  auto func = [aChannel, aSuspend](AudioChannelAgent* agent) {
    if (agent->AudioChannelType() == static_cast<int32_t>(aChannel)) {
      agent->WindowSuspendChanged(aSuspend);
    }
  };
  RefreshAgents(aWindow, func);

#ifdef MOZ_WIDGET_GONK
  // On the contrary, if our audio channel activity is switched from
  // active to inactive, we need to refresh SpeakerManagers after
  // AudioChannelAgents are muted.
  if (!active) {
    for (auto* service : mSpeakerManagerServices) {
      service->SetAudioChannelActive(active);
    }
  }
#endif
}

void AudioChannelService::RefreshAgentsVolumeAndPropagate(
    nsPIDOMWindowOuter* aWindow, AudioChannel aChannel, float aVolume,
    bool aMuted) {
  for (auto& browserParent : mBrowserParents) {
    nsCOMPtr<nsPIDOMWindowOuter> topWindow = GetTopAppWindow(browserParent);
    if (topWindow == aWindow) {
      Unused << browserParent->SendSetAudioChannelVolume(
          static_cast<uint32_t>(aChannel), aVolume, aMuted);
    }
  }
  RefreshAgentsVolume(aWindow, aChannel, aVolume, aMuted);
}

void AudioChannelService::RefreshAgentsSuspendAndPropagate(
    nsPIDOMWindowOuter* aWindow, AudioChannel aChannel,
    nsSuspendedTypes aSuspend) {
  for (auto& browserParent : mBrowserParents) {
    nsCOMPtr<nsPIDOMWindowOuter> topWindow = GetTopAppWindow(browserParent);
    if (topWindow == aWindow) {
      Unused << browserParent->SendSetAudioChannelSuspend(
          static_cast<uint32_t>(aChannel), aSuspend);
    }
  }
  RefreshAgentsSuspend(aWindow, aChannel, aSuspend);
}

void AudioChannelService::SetWindowAudioCaptured(nsPIDOMWindowOuter* aWindow,
                                                 uint64_t aInnerWindowID,
                                                 bool aCapture) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWindow);

  MOZ_LOG(GetAudioChannelLog(), LogLevel::Debug,
          ("AudioChannelService, SetWindowAudioCaptured, window = %p, "
           "aCapture = %d\n",
           aWindow, aCapture));

  nsCOMPtr<nsPIDOMWindowOuter> topWindow = aWindow->GetInProcessScriptableTop();
  if (!topWindow) {
    return;
  }

  AudioChannelWindow* winData = GetWindowData(topWindow->WindowID());

  // This can happen, but only during shutdown, because the the outer window
  // changes ScriptableTop, so that its ID is different.
  // In this case either we are capturing, and it's too late because the window
  // has been closed anyways, or we are un-capturing, and everything has already
  // been cleaned up by the HTMLMediaElements or the AudioContexts.
  if (!winData) {
    return;
  }

  if (aCapture != winData->mIsAudioCaptured) {
    winData->mIsAudioCaptured = aCapture;
    for (AudioChannelAgent* agent : winData->mAgents.ForwardRange()) {
      agent->WindowAudioCaptureChanged(aInnerWindowID, aCapture);
    }
  }
}

/* static */
const nsAttrValue::EnumTable* AudioChannelService::GetAudioChannelTable() {
  return kMozAudioChannelAttributeTable;
}

/* static */
AudioChannel AudioChannelService::GetAudioChannel(const nsAString& aChannel) {
  for (uint32_t i = 0; kMozAudioChannelAttributeTable[i].tag; ++i) {
    if (aChannel.EqualsASCII(kMozAudioChannelAttributeTable[i].tag)) {
      return static_cast<AudioChannel>(kMozAudioChannelAttributeTable[i].value);
    }
  }

  return AudioChannel::Normal;
}

/* static */
AudioChannel AudioChannelService::GetDefaultAudioChannel() {
  nsAutoString audioChannel;
  Preferences::GetString("media.defaultAudioChannel", audioChannel);
  if (audioChannel.IsEmpty()) {
    return AudioChannel::Normal;
  }

  for (uint32_t i = 0; kMozAudioChannelAttributeTable[i].tag; ++i) {
    if (audioChannel.EqualsASCII(kMozAudioChannelAttributeTable[i].tag)) {
      return static_cast<AudioChannel>(kMozAudioChannelAttributeTable[i].value);
    }
  }

  return AudioChannel::Normal;
}

/* static */
void AudioChannelService::GetAudioChannelString(AudioChannel aChannel,
                                                nsAString& aString) {
  aString.AssignASCII("normal");

  for (uint32_t i = 0; kMozAudioChannelAttributeTable[i].tag; ++i) {
    if (aChannel ==
        static_cast<AudioChannel>(kMozAudioChannelAttributeTable[i].value)) {
      aString.AssignASCII(kMozAudioChannelAttributeTable[i].tag);
      break;
    }
  }
}

/* static */
void AudioChannelService::GetDefaultAudioChannelString(nsAString& aString) {
  aString.AssignASCII("normal");

  nsAutoString audioChannel;
  Preferences::GetString("media.defaultAudioChannel", audioChannel);
  if (!audioChannel.IsEmpty()) {
    for (uint32_t i = 0; kMozAudioChannelAttributeTable[i].tag; ++i) {
      if (audioChannel.EqualsASCII(kMozAudioChannelAttributeTable[i].tag)) {
        aString = audioChannel;
        break;
      }
    }
  }
}

AudioChannelService::AudioChannelWindow*
AudioChannelService::GetOrCreateWindowData(nsPIDOMWindowOuter* aWindow) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWindow);

  AudioChannelWindow* winData = GetWindowData(aWindow->WindowID());
  if (!winData) {
    winData = new AudioChannelWindow(aWindow->WindowID());
    mWindows.AppendElement(WrapUnique(winData));
  }

  return winData;
}

AudioChannelService::AudioChannelWindow* AudioChannelService::GetWindowData(
    uint64_t aWindowID) const {
  const auto [begin, end] = mWindows.NonObservingRange();
  const auto foundIt = std::find_if(begin, end, [aWindowID](const auto& next) {
    return next->mWindowID == aWindowID;
  });
  return foundIt != end ? foundIt->get() : nullptr;
}

AudioChannelService::AudioChannelConfig* AudioChannelService::GetChannelConfig(
    nsPIDOMWindowOuter* aWindow, AudioChannel aChannel) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWindow);

  AudioChannelWindow* winData = GetOrCreateWindowData(aWindow);
  return &winData->mChannels[static_cast<uint32_t>(aChannel)];
}

bool AudioChannelService::GetAudioChannelVolume(nsPIDOMWindowOuter* aWindow,
                                                AudioChannel aChannel,
                                                float& aVolume, bool& aMuted) {
  AudioChannelConfig* config = GetChannelConfig(aWindow, aChannel);
  aVolume = config->mVolume;
  aMuted = config->mMuted;
  return true;
}

bool AudioChannelService::SetAudioChannelVolume(nsPIDOMWindowOuter* aWindow,
                                                AudioChannel aChannel,
                                                float aVolume, bool aMuted) {
  MOZ_LOG(GetAudioChannelLog(), LogLevel::Debug,
          ("AudioChannelService, SetAudioChannelVolume, window = %p, "
           "type = %" PRIu32 ", volume = %f, mute = %s\n",
           aWindow, static_cast<uint32_t>(aChannel), aVolume,
           aMuted ? "true" : "false"));

  AudioChannelConfig* config = GetChannelConfig(aWindow, aChannel);
  config->mVolume = aVolume;
  config->mMuted = aMuted;
  RefreshAgentsVolumeAndPropagate(aWindow, aChannel, aVolume, aMuted);
  return true;
}

bool AudioChannelService::GetAudioChannelSuspend(nsPIDOMWindowOuter* aWindow,
                                                 AudioChannel aChannel,
                                                 nsSuspendedTypes& aSuspend) {
  aSuspend = GetChannelConfig(aWindow, aChannel)->mSuspend;
  return true;
}

bool AudioChannelService::SetAudioChannelSuspend(nsPIDOMWindowOuter* aWindow,
                                                 AudioChannel aChannel,
                                                 nsSuspendedTypes aSuspend) {
  MOZ_LOG(GetAudioChannelLog(), LogLevel::Debug,
          ("AudioChannelService, SetAudioChannelSuspend, window = %p, "
           "type = %" PRIu32 ", suspend = %" PRIu32 "\n",
           aWindow, static_cast<uint32_t>(aChannel), aSuspend));

  GetChannelConfig(aWindow, aChannel)->mSuspend = aSuspend;
  MaybeSendStatusUpdate();
  RefreshAgentsSuspendAndPropagate(aWindow, aChannel, aSuspend);
  return true;
}

bool AudioChannelService::IsAudioChannelActive(nsPIDOMWindowOuter* aWindow,
                                               AudioChannel aChannel) {
  return !!GetChannelConfig(aWindow, aChannel)->mNumberOfAgents;
}

bool AudioChannelService::IsWindowActive(nsPIDOMWindowOuter* aWindow) {
  MOZ_ASSERT(NS_IsMainThread());

  auto* window = nsPIDOMWindowOuter::From(aWindow)->GetInProcessScriptableTop();
  if (!window) {
    return false;
  }

  AudioChannelWindow* winData = GetWindowData(window->WindowID());
  if (!winData) {
    return false;
  }

  return !winData->mAudibleAgents.IsEmpty();
}

void AudioChannelService::SetDefaultVolumeControlChannel(int32_t aChannel,
                                                         bool aVisible) {
  SetDefaultVolumeControlChannelInternal(aChannel, aVisible,
                                         hal::CONTENT_PROCESS_ID_MAIN);
}

void AudioChannelService::SetDefaultVolumeControlChannelInternal(
    int32_t aChannel, bool aVisible, uint64_t aChildID) {
  if (!XRE_IsParentProcess()) {
    ContentChild* cc = ContentChild::GetSingleton();
    if (cc) {
      cc->SendAudioChannelChangeDefVolChannel(aChannel, aVisible);
    }

    return;
  }

  // If this child is in the background and mDefChannelChildID is set to
  // others then it means other child in the foreground already set it's
  // own default channel.
  if (!aVisible && mDefChannelChildID != aChildID) {
    return;
  }

  // Workaround for the call screen app. The call screen app is running on the
  // main process, that will results in wrong visible state. Because we use the
  // docshell's active state as visible state, the main process is always
  // active. Therefore, we will see the strange situation that the visible
  // state of the call screen is always true. If the mDefChannelChildID is set
  // to others then it means other child in the foreground already set it's
  // own default channel already.
  // Summary :
  //   Child process : foreground app always can set type.
  //   Parent process : check the mDefChannelChildID.
  else if (aChildID == hal::CONTENT_PROCESS_ID_MAIN &&
           mDefChannelChildID != hal::CONTENT_PROCESS_ID_UNKNOWN) {
    return;
  }

  mDefChannelChildID = aVisible ? aChildID : hal::CONTENT_PROCESS_ID_UNKNOWN;
  nsAutoString channelName;

  if (aChannel == -1) {
    channelName.AssignASCII("unknown");
  } else {
    GetAudioChannelString(static_cast<AudioChannel>(aChannel), channelName);
  }

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->NotifyObservers(nullptr, "default-volume-channel-changed",
                         channelName.get());
  }
}

void AudioChannelService::MaybeSendStatusUpdate() {
  if (XRE_IsParentProcess()) {
    return;
  }

  bool telephonyChannel = TelephonyChannelIsActive();
  bool contentOrNormalChannel = ContentOrNormalChannelIsActive();
  bool anyChannel = AnyAudioChannelIsActive();

  if (telephonyChannel == mTelephonyChannel &&
      contentOrNormalChannel == mContentOrNormalChannel &&
      anyChannel == mAnyChannel) {
    return;
  }

  mTelephonyChannel = telephonyChannel;
  mContentOrNormalChannel = contentOrNormalChannel;
  mAnyChannel = anyChannel;

  ContentChild* cc = ContentChild::GetSingleton();
  if (cc) {
    cc->SendAudioChannelServiceStatus(telephonyChannel, contentOrNormalChannel,
                                      anyChannel);
  }
}

void AudioChannelService::ChildStatusReceived(uint64_t aChildID,
                                              bool aTelephonyChannel,
                                              bool aContentOrNormalChannel,
                                              bool aAnyChannel) {
  if (!aAnyChannel) {
    RemoveChildStatus(aChildID);
    return;
  }

  AudioChannelChildStatus* data = GetChildStatus(aChildID);
  if (!data) {
    data = new AudioChannelChildStatus(aChildID);
    mPlayingChildren.AppendElement(WrapUnique(data));
  }

  data->mActiveTelephonyChannel = aTelephonyChannel;
  data->mActiveContentOrNormalChannel = aContentOrNormalChannel;
}

void AudioChannelService::NotifyResumingDelayedMedia(
    nsPIDOMWindowOuter* aWindow) {
  MOZ_ASSERT(aWindow);

  nsCOMPtr<nsPIDOMWindowOuter> topWindow = aWindow->GetInProcessScriptableTop();
  if (!topWindow) {
    return;
  }

  AudioChannelWindow* winData = GetWindowData(topWindow->WindowID());
  if (!winData) {
    return;
  }

  winData->NotifyMediaBlockStop(aWindow);
  // B2G audio channel should be always controlled by system app.
#ifndef MOZ_WIDGET_GONK
  RefreshAgentsSuspend(aWindow, AudioChannel::Normal,
                       nsISuspendedTypes::NONE_SUSPENDED);
#endif
}

/* static */
nsSuspendedTypes AudioChannelService::InitialSuspendType() {
  CreateServiceIfNeeded();
  return StaticPrefs::dom_audiochannel_mutedByDefault()
             ? nsISuspendedTypes::SUSPENDED_BLOCK
             : nsISuspendedTypes::NONE_SUSPENDED;
}

void AudioChannelService::AudioChannelWindow::AppendAgent(
    AudioChannelAgent* aAgent, AudibleState aAudible) {
  MOZ_ASSERT(aAgent);

  AppendAgentAndIncreaseAgentsNum(aAgent);
  AudioAudibleChanged(aAgent, aAudible,
                      AudibleChangedReasons::eDataAudibleChanged);
}

void AudioChannelService::AudioChannelWindow::RemoveAgent(
    AudioChannelAgent* aAgent) {
  MOZ_ASSERT(aAgent);

  RemoveAgentAndReduceAgentsNum(aAgent);
  AudioAudibleChanged(aAgent, AudibleState::eNotAudible,
                      AudibleChangedReasons::ePauseStateChanged);
}

void AudioChannelService::AudioChannelWindow::NotifyMediaBlockStop(
    nsPIDOMWindowOuter* aWindow) {
  if (mShouldSendActiveMediaBlockStopEvent) {
    mShouldSendActiveMediaBlockStopEvent = false;
    nsCOMPtr<nsPIDOMWindowOuter> window = aWindow;
    NS_DispatchToCurrentThread(NS_NewRunnableFunction(
        "dom::AudioChannelService::AudioChannelWindow::NotifyMediaBlockStop",
        [window]() -> void {
          nsCOMPtr<nsIObserverService> observerService =
              services::GetObserverService();
          if (NS_WARN_IF(!observerService)) {
            return;
          }

          observerService->NotifyObservers(ToSupports(window), "audio-playback",
                                           u"activeMediaBlockStop");
        }));
  }
}

bool AudioChannelService::AudioChannelWindow::IsChannelActive(
    AudioChannel aChannel) {
  uint32_t channel = static_cast<uint32_t>(aChannel);
  MOZ_ASSERT(channel < NUMBER_OF_AUDIO_CHANNELS);
  return mChannels[channel].mNumberOfAgents != 0 &&
         mChannels[channel].mSuspend == nsISuspendedTypes::NONE_SUSPENDED;
}

void AudioChannelService::AudioChannelWindow::AppendAgentAndIncreaseAgentsNum(
    AudioChannelAgent* aAgent) {
  MOZ_ASSERT(aAgent);
  MOZ_ASSERT(!mAgents.Contains(aAgent));

  int32_t channel = aAgent->AudioChannelType();
  mAgents.AppendElement(aAgent);

  ++mChannels[channel].mNumberOfAgents;

  // The first one, we must inform the AudioChannelHandler.
  if (mChannels[channel].mNumberOfAgents == 1) {
    NotifyChannelActive(aAgent->WindowID(), static_cast<AudioChannel>(channel),
                        true);
  }
}

void AudioChannelService::AudioChannelWindow::RemoveAgentAndReduceAgentsNum(
    AudioChannelAgent* aAgent) {
  MOZ_ASSERT(aAgent);
  MOZ_ASSERT(mAgents.Contains(aAgent));

  int32_t channel = aAgent->AudioChannelType();
  mAgents.RemoveElement(aAgent);

  auto reduceAgentsNum = [this, channel, windowID = aAgent->WindowID()]() {
    if (sXPCOMShuttingDown) {
      return;
    }
    MOZ_ASSERT(mChannels[channel].mNumberOfAgents > 0);
    --mChannels[channel].mNumberOfAgents;

    if (mChannels[channel].mNumberOfAgents == 0) {
      NotifyChannelActive(windowID, static_cast<AudioChannel>(channel), false);
    }
  };

  uint32_t delayMs = StaticPrefs::dom_audiochannel_release_delay_ms();
  if (delayMs == 0) {
    reduceAgentsNum();
  } else {
    MOZ_LOG(AudioChannelService::GetAudioChannelLog(), LogLevel::Debug,
            ("AudioChannelService, delay releasing channel by %u ms", delayMs));
    NS_DelayedDispatchToCurrentThread(
        NS_NewRunnableFunction("AudioChannelService::AudioChannelWindow::"
                               "RemoveAgentAndReduceAgentsNum",
                               reduceAgentsNum),
        delayMs);
  }
}

void AudioChannelService::AudioChannelWindow::AudioAudibleChanged(
    AudioChannelAgent* aAgent, AudibleState aAudible,
    AudibleChangedReasons aReason) {
  MOZ_ASSERT(aAgent);

  if (aAudible == AudibleState::eAudible) {
    AppendAudibleAgentIfNotContained(aAgent, aReason);
  } else {
    RemoveAudibleAgentIfContained(aAgent, aReason);
  }

  if (aAudible != AudibleState::eNotAudible) {
    MaybeNotifyMediaBlockStart(aAgent);
  }
}

void AudioChannelService::AudioChannelWindow::AppendAudibleAgentIfNotContained(
    AudioChannelAgent* aAgent, AudibleChangedReasons aReason) {
  MOZ_ASSERT(aAgent);
  MOZ_ASSERT(mAgents.Contains(aAgent));

  if (!mAudibleAgents.Contains(aAgent)) {
    mAudibleAgents.AppendElement(aAgent);
    if (IsFirstAudibleAgent()) {
      NotifyAudioAudibleChanged(aAgent->Window(), AudibleState::eAudible,
                                aReason);
    }
  }
}

void AudioChannelService::AudioChannelWindow::RemoveAudibleAgentIfContained(
    AudioChannelAgent* aAgent, AudibleChangedReasons aReason) {
  MOZ_ASSERT(aAgent);

  if (mAudibleAgents.Contains(aAgent)) {
    mAudibleAgents.RemoveElement(aAgent);
    if (IsLastAudibleAgent()) {
      NotifyAudioAudibleChanged(aAgent->Window(), AudibleState::eNotAudible,
                                aReason);
    }
  }
}

bool AudioChannelService::AudioChannelWindow::IsFirstAudibleAgent() const {
  return (mAudibleAgents.Length() == 1);
}

bool AudioChannelService::AudioChannelWindow::IsLastAudibleAgent() const {
  return mAudibleAgents.IsEmpty();
}

void AudioChannelService::AudioChannelWindow::NotifyAudioAudibleChanged(
    nsPIDOMWindowOuter* aWindow, AudibleState aAudible,
    AudibleChangedReasons aReason) {
  RefPtr<AudioPlaybackRunnable> runnable = new AudioPlaybackRunnable(
      aWindow, aAudible == AudibleState::eAudible, aReason);
  DebugOnly<nsresult> rv = NS_DispatchToCurrentThread(runnable);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "NS_DispatchToCurrentThread failed");
}

void AudioChannelService::AudioChannelWindow::NotifyChannelActive(
    uint64_t aWindowID, AudioChannel aChannel, bool aActive) {
  RefPtr<NotifyChannelActiveRunnable> runnable =
      new NotifyChannelActiveRunnable(aWindowID, aChannel, aActive);
  DebugOnly<nsresult> rv = NS_DispatchToCurrentThread(runnable);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "NS_DispatchToCurrentThread failed");
}

void AudioChannelService::AudioChannelWindow::MaybeNotifyMediaBlockStart(
    AudioChannelAgent* aAgent) {
  nsCOMPtr<nsPIDOMWindowOuter> window = aAgent->Window();
  if (!window) {
    return;
  }

  nsCOMPtr<nsPIDOMWindowInner> inner = window->GetCurrentInnerWindow();
  if (!inner) {
    return;
  }

  nsCOMPtr<Document> doc = inner->GetExtantDoc();
  if (!doc) {
    return;
  }

  if (!window->ShouldDelayMediaFromStart() || !doc->Hidden()) {
    return;
  }

  if (!mShouldSendActiveMediaBlockStopEvent) {
    mShouldSendActiveMediaBlockStopEvent = true;
    NS_DispatchToCurrentThread(NS_NewRunnableFunction(
        "dom::AudioChannelService::AudioChannelWindow::"
        "MaybeNotifyMediaBlockStart",
        [window]() -> void {
          nsCOMPtr<nsIObserverService> observerService =
              services::GetObserverService();
          if (NS_WARN_IF(!observerService)) {
            return;
          }

          observerService->NotifyObservers(ToSupports(window), "audio-playback",
                                           u"activeMediaBlockStart");
        }));
  }
}

}  // namespace mozilla::dom
