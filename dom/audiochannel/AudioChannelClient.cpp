/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioChannelClient.h"

#include "AudioChannelService.h"
#include "nsIPermissionManager.h"
#include "nsIScriptObjectPrincipal.h"
#include "mozilla/Services.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(AudioChannelClient, DOMEventTargetHelper,
                                   mAgent)

NS_IMPL_ADDREF_INHERITED(AudioChannelClient, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(AudioChannelClient, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AudioChannelClient)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsIAudioChannelAgentCallback)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

/* static */
already_AddRefed<AudioChannelClient> AudioChannelClient::Constructor(
    const GlobalObject& aGlobal, AudioChannel aChannel, ErrorResult& aRv) {
  nsCOMPtr<nsPIDOMWindowInner> window =
      do_QueryInterface(aGlobal.GetAsSupports());
  if (!window) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  if (!CheckAudioChannelPermissions(window, aChannel)) {
    aRv.ThrowSecurityError("Permission denied"_ns);
    return nullptr;
  }

  RefPtr<AudioChannelClient> object = new AudioChannelClient(window, aChannel);
  aRv = NS_OK;
  return object.forget();
}

AudioChannelClient::AudioChannelClient(nsPIDOMWindowInner* aWindow,
                                       AudioChannel aChannel)
    : DOMEventTargetHelper(aWindow), mChannel(aChannel), mSuspended(true) {
  MOZ_LOG(AudioChannelService::GetAudioChannelLog(), LogLevel::Debug,
          ("AudioChannelClient, created with channel type %d", (int)aChannel));
}

AudioChannelClient::~AudioChannelClient() {
  MOZ_LOG(AudioChannelService::GetAudioChannelLog(), LogLevel::Debug,
          ("AudioChannelClient, destroyed"));
}

/* static */
bool AudioChannelClient::CheckAudioChannelPermissions(
    nsPIDOMWindowInner* aWindow, AudioChannel aChannel) {
  // Only normal channel doesn't need permission.
  if (aChannel == AudioChannel::Normal) {
    return true;
  }

  // Maybe this audio channel is equal to the default one.
  if (aChannel == AudioChannelService::GetDefaultAudioChannel()) {
    return true;
  }

  nsCOMPtr<nsIPermissionManager> permissionManager =
      mozilla::services::GetPermissionManager();
  if (!permissionManager) {
    return false;
  }

  nsCOMPtr<nsIScriptObjectPrincipal> sop = do_QueryInterface(aWindow);
  NS_ASSERTION(sop, "Window didn't QI to nsIScriptObjectPrincipal!");
  nsCOMPtr<nsIPrincipal> principal = sop->GetPrincipal();

  uint32_t perm = nsIPermissionManager::UNKNOWN_ACTION;

  nsAutoCString channel("audio-channel-");
  channel.AppendASCII(AudioChannelValues::strings[uint32_t(aChannel)].value,
                      AudioChannelValues::strings[uint32_t(aChannel)].length);
  permissionManager->TestExactPermissionFromPrincipal(principal, channel,
                                                      &perm);

  return perm == nsIPermissionManager::ALLOW_ACTION;
}

JSObject* AudioChannelClient::WrapObject(JSContext* aCx,
                                         JS::Handle<JSObject*> aGivenProto) {
  return AudioChannelClient_Binding::Wrap(aCx, this, aGivenProto);
}

void AudioChannelClient::RequestChannel(ErrorResult& aRv) {
  if (mAgent) {
    return;
  }

  MOZ_LOG(AudioChannelService::GetAudioChannelLog(), LogLevel::Debug,
          ("AudioChannelClient, request channel"));

  mAgent = new AudioChannelAgent();
  aRv = mAgent->InitWithWeakCallback(GetOwner(), static_cast<int32_t>(mChannel),
                                     this);
  if (NS_WARN_IF(aRv.Failed())) {
    mAgent = nullptr;
    return;
  }

  aRv = mAgent->NotifyStartedPlaying(
      AudioChannelService::AudibleState::eNotAudible);
  if (NS_WARN_IF(aRv.Failed())) {
    mAgent = nullptr;
    return;
  }

  mAgent->PullInitialUpdate();
}

void AudioChannelClient::AbandonChannel(ErrorResult& aRv) {
  if (!mAgent) {
    return;
  }

  MOZ_LOG(AudioChannelService::GetAudioChannelLog(), LogLevel::Debug,
          ("AudioChannelClient, abandon channel"));

  mAgent->NotifyStoppedPlaying();
  mAgent = nullptr;
  mSuspended = true;
}

NS_IMETHODIMP
AudioChannelClient::WindowVolumeChanged(float aVolume, bool aMuted) {
  return NS_OK;
}

NS_IMETHODIMP
AudioChannelClient::WindowSuspendChanged(nsSuspendedTypes aSuspend) {
  bool suspended = aSuspend != nsISuspendedTypes::NONE_SUSPENDED;
  if (mSuspended != suspended) {
    mSuspended = suspended;
    MOZ_LOG(AudioChannelService::GetAudioChannelLog(), LogLevel::Debug,
            ("AudioChannelClient, state changed, suspended %d", mSuspended));
    DispatchTrustedEvent(u"statechange"_ns);
  }
  return NS_OK;
}

NS_IMETHODIMP
AudioChannelClient::WindowAudioCaptureChanged(bool aCapture) { return NS_OK; }

}  // namespace dom
}  // namespace mozilla
