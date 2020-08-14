/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Document.h"
#include "nsIDOMEventListener.h"
#include "nsPIDOMWindow.h"
#include "nsIDocShell.h"
#include "nsIPermissionManager.h"
#include "nsIInterfaceRequestorUtils.h"
#include "AudioChannelManager.h"
#include "mozilla/dom/AudioChannelHandler.h"
#include "mozilla/dom/AudioChannelManagerBinding.h"
#include "mozilla/dom/Event.h"
#include "mozilla/Services.h"

namespace mozilla {
namespace dom {
namespace system {

NS_IMPL_QUERY_INTERFACE_INHERITED(AudioChannelManager, DOMEventTargetHelper,
                                  nsIDOMEventListener)
NS_IMPL_ADDREF_INHERITED(AudioChannelManager, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(AudioChannelManager, DOMEventTargetHelper)

AudioChannelManager::AudioChannelManager() : mVolumeChannel(-1) {
  hal::RegisterSwitchObserver(hal::SWITCH_HEADPHONES, this);
}

AudioChannelManager::~AudioChannelManager() {
  hal::UnregisterSwitchObserver(hal::SWITCH_HEADPHONES, this);

  nsCOMPtr<EventTarget> target = do_QueryInterface(GetOwner());
  NS_ENSURE_TRUE_VOID(target);

  target->RemoveSystemEventListener(u"visibilitychange"_ns, this,
                                    /* useCapture = */ true);
}

void AudioChannelManager::Init(nsIGlobalObject* aGlobal) {
  BindToOwner(aGlobal);

  nsCOMPtr<EventTarget> target = do_QueryInterface(GetOwner());
  NS_ENSURE_TRUE_VOID(target);

  target->AddSystemEventListener(u"visibilitychange"_ns, this,
                                 /* useCapture = */ true,
                                 /* wantsUntrusted = */ false);
}

JSObject* AudioChannelManager::WrapObject(JSContext* aCx,
                                          JS::Handle<JSObject*> aGivenProto) {
  return AudioChannelManager_Binding::Wrap(aCx, this, aGivenProto);
}

void AudioChannelManager::Notify(const hal::SwitchEvent& aEvent) {
  mState = Some(aEvent.status());

  DispatchTrustedEvent(u"headphoneschange"_ns);
}

void AudioChannelManager::SetVolumeControlChannel(const nsAString& aChannel) {
  if (aChannel.EqualsASCII("publicnotification")) {
    return;
  }

  AudioChannel newChannel = AudioChannelService::GetAudioChannel(aChannel);

  /* FIXME
  // Only normal channel doesn't need permission.
  if (newChannel != AudioChannel::Normal) {
    nsCOMPtr<nsIPermissionManager> permissionManager =
        services::GetPermissionManager();
    if (!permissionManager) {
      return;
    }
    uint32_t perm = nsIPermissionManager::UNKNOWN_ACTION;
    nsAutoCString channel;
    channel.Assign("audio-channel-");
    channel.Append(NS_ConvertUTF16toUTF8(aChannel));
    permissionManager->TestPermissionFromWindow(GetOwner(), channel, &perm);
    if (perm != nsIPermissionManager::ALLOW_ACTION) {
      return;
    }
  }
  */

  if (mVolumeChannel == (int32_t)newChannel) {
    return;
  }

  mVolumeChannel = (int32_t)newChannel;

  NotifyVolumeControlChannelChanged();
}

void AudioChannelManager::GetVolumeControlChannel(nsAString& aChannel) {
  if (mVolumeChannel >= 0) {
    AudioChannelService::GetAudioChannelString(
        static_cast<AudioChannel>(mVolumeChannel), aChannel);
  } else {
    aChannel.AssignASCII("");
  }
}

void AudioChannelManager::NotifyVolumeControlChannelChanged() {
  nsCOMPtr<nsIDocShell> docshell = do_GetInterface(GetOwner());
  NS_ENSURE_TRUE_VOID(docshell);

  bool isActive = false;
  docshell->GetIsActive(&isActive);

  RefPtr<AudioChannelService> service = AudioChannelService::GetOrCreate();
  if (!service) {
    return;
  }

  if (isActive) {
    service->SetDefaultVolumeControlChannel(mVolumeChannel, isActive);
  } else {
    service->SetDefaultVolumeControlChannel(-1, isActive);
  }
}

NS_IMETHODIMP
AudioChannelManager::HandleEvent(Event* aEvent) {
  nsAutoString type;
  aEvent->GetType(type);

  if (type.EqualsLiteral("visibilitychange")) {
    NotifyVolumeControlChannelChanged();
  }
  return NS_OK;
}

void AudioChannelManager::GetAllowedAudioChannels(
    nsTArray<RefPtr<AudioChannelHandler>>& aAudioChannels, ErrorResult& aRv) {
  MOZ_ASSERT(aAudioChannels.IsEmpty());

  // Only main process is supported.
  if (XRE_GetProcessType() != GeckoProcessType_Default) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsCOMPtr<nsPIDOMWindowInner> window = GetOwner();
  if (NS_WARN_IF(!window)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  AudioChannelHandler::GenerateAllowedChannels(window, aAudioChannels, aRv);
  NS_WARNING_ASSERTION(!aRv.Failed(), "GenerateAllowedAudioChannels failed");
}

}  // namespace system
}  // namespace dom
}  // namespace mozilla
