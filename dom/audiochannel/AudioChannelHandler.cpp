/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioChannelHandler.h"

#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/dom/AudioChannelHandlerBinding.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Promise.h"
#include "AudioChannelService.h"
#include "nsIDocShell.h"
#include "nsIObserverService.h"
#include "nsISupportsPrimitives.h"
#include "nsPIDOMWindow.h"

namespace mozilla {
namespace dom {

NS_IMPL_ADDREF_INHERITED(AudioChannelHandler, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(AudioChannelHandler, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AudioChannelHandler)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_CYCLE_COLLECTION_INHERITED(AudioChannelHandler, DOMEventTargetHelper,
                                   mFrameLoader, mBrowserParent, mFrameWindow)

/* static */
already_AddRefed<AudioChannelHandler> AudioChannelHandler::Create(
    nsPIDOMWindowInner* aParentWindow, nsFrameLoader* aFrameLoader,
    AudioChannel aAudioChannel, ErrorResult& aRv) {
  RefPtr<AudioChannelHandler> ac =
      new AudioChannelHandler(aParentWindow, aFrameLoader, aAudioChannel);

  aRv = ac->Initialize();
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  MOZ_LOG(AudioChannelService::GetAudioChannelLog(), LogLevel::Debug,
          ("AudioChannelHandler, Create, channel = %p, type = %" PRIu32 "\n",
           ac.get(), static_cast<uint32_t>(aAudioChannel)));

  return ac.forget();
}

/* static */
void AudioChannelHandler::GenerateAllowedChannelsInternal(
    nsPIDOMWindowInner* aParentWindow, nsFrameLoader* aFrameLoader,
    nsTArray<RefPtr<AudioChannelHandler>>& aAudioChannels, ErrorResult& aRv) {
  MOZ_ASSERT(aAudioChannels.IsEmpty());

  nsPIDOMWindowInner* parentWindow = aParentWindow;
  if (aFrameLoader) {
    nsCOMPtr<Element> frameElement = aFrameLoader->GetOwnerElement();
    if (!frameElement) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }

    nsCOMPtr<Document> doc = frameElement->GetOwnerDocument();
    if (!doc) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }

    nsPIDOMWindowOuter* window = doc->GetWindow();
    if (!window) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }

    parentWindow = window->GetCurrentInnerWindow();
  }

  nsTArray<RefPtr<AudioChannelHandler>> channels;

  const nsAttrValue::EnumTable* audioChannelTable =
      AudioChannelService::GetAudioChannelTable();

  for (uint32_t i = 0; audioChannelTable && audioChannelTable[i].tag; ++i) {
    // FIXME: check permission for each channel here.

    AudioChannel value = (AudioChannel)audioChannelTable[i].value;
    RefPtr<AudioChannelHandler> ac =
        AudioChannelHandler::Create(parentWindow, aFrameLoader, value, aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }

    channels.AppendElement(ac);
  }

  aAudioChannels.SwapElements(channels);
}

AudioChannelHandler::AudioChannelHandler(nsPIDOMWindowInner* aParentWindow,
                                         nsFrameLoader* aFrameLoader,
                                         AudioChannel aAudioChannel)
    : DOMEventTargetHelper(aParentWindow),
      mFrameLoader(aFrameLoader),
      mAudioChannel(aAudioChannel),
      mState(eStateUnknown) {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    nsAutoString name;
    AudioChannelService::GetAudioChannelString(aAudioChannel, name);

    nsAutoCString topic;
    topic.Assign("audiochannel-activity-");
    topic.Append(NS_ConvertUTF16toUTF8(name));

    obs->AddObserver(this, topic.get(), true);
  }
}

AudioChannelHandler::~AudioChannelHandler() {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    nsAutoString name;
    AudioChannelService::GetAudioChannelString(mAudioChannel, name);

    nsAutoCString topic;
    topic.Assign("audiochannel-activity-");
    topic.Append(NS_ConvertUTF16toUTF8(name));

    obs->RemoveObserver(this, topic.get());
  }
}

nsresult AudioChannelHandler::Initialize() {
  if (!mFrameLoader) {
    nsCOMPtr<nsPIDOMWindowInner> window = GetOwner();
    if (!window) {
      return NS_ERROR_FAILURE;
    }
    mFrameWindow = window->GetInProcessScriptableTop();
    return NS_OK;
  }

  nsCOMPtr<nsIDocShell> docShell = mFrameLoader->GetDocShell(IgnoreErrors());
  if (docShell) {
    nsCOMPtr<nsPIDOMWindowOuter> window = docShell->GetWindow();
    if (!window) {
      return NS_ERROR_FAILURE;
    }
    mFrameWindow = window->GetInProcessScriptableTop();
    return NS_OK;
  }

  mBrowserParent = mFrameLoader->GetBrowserParent();
  MOZ_ASSERT(mBrowserParent);
  return NS_OK;
}

JSObject* AudioChannelHandler::WrapObject(JSContext* aCx,
                                          JS::Handle<JSObject*> aGivenProto) {
  return AudioChannelHandler_Binding::Wrap(aCx, this, aGivenProto);
}

AudioChannel AudioChannelHandler::Name() const {
  MOZ_ASSERT(NS_IsMainThread());
  return mAudioChannel;
}

namespace {

// Helpers for promise resolve/reject
template <typename T>
void ResolveOrReject(RefPtr<Promise> aPromise, bool aSuccess, T aValue) {
  MOZ_ASSERT(aPromise);
  if (aSuccess) {
    aPromise->MaybeResolve(aValue);
  } else {
    aPromise->MaybeRejectWithOperationError(
        "Unable to perform audio channel handler operations");
  }
}

void ResolveOrReject(RefPtr<Promise> aPromise, bool aSuccess) {
  MOZ_ASSERT(aPromise);
  if (aSuccess) {
    aPromise->MaybeResolveWithUndefined();
  } else {
    aPromise->MaybeRejectWithOperationError(
        "Unable to perform audio channel handler operations");
  }
}

// Helpers for default IPC resolve/reject callbacks. The default resolve has no
// returned value and takes just one bool parameter of the IPC operation's
// successfulness.
typedef mozilla::ipc::ResolveCallback<bool> DefaultIpcResolve;
typedef mozilla::ipc::RejectCallback DefaultIpcReject;

DefaultIpcResolve GetDefaultIpcResolve(RefPtr<Promise>& aPromise) {
  return [aPromise](bool&& aSuccess) { ResolveOrReject(aPromise, aSuccess); };
}

DefaultIpcReject GetDefaultIpcReject(RefPtr<Promise>& aPromise) {
  return [aPromise](mozilla::ipc::ResponseRejectReason) {
    ResolveOrReject(aPromise, false);
  };
}

// Helpers for suspended types.
bool IsSuspended(nsSuspendedTypes aSuspend) {
  return aSuspend != nsISuspendedTypes::NONE_SUSPENDED;
}

}  // anonymous namespace

already_AddRefed<Promise> AudioChannelHandler::GetVolume(ErrorResult& aRv) {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIGlobalObject> parentObject = do_QueryInterface(GetParentObject());
  RefPtr<Promise> promise = Promise::Create(parentObject, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  if (IsRemoteFrame()) {
    auto resolve = [promise](std::tuple<bool, float, bool>&& aResult) {
      bool success = std::get<0>(aResult);
      float volume = std::get<1>(aResult);
      ResolveOrReject(promise, success, volume);
    };
    mBrowserParent->SendGetAudioChannelVolume(
        static_cast<uint32_t>(mAudioChannel), resolve,
        GetDefaultIpcReject(promise));
  } else {
    float volume = 0.0;
    bool muted = false;
    bool success = false;
    RefPtr<AudioChannelService> service = AudioChannelService::GetOrCreate();
    if (service) {
      success = service->GetAudioChannelVolume(mFrameWindow, mAudioChannel,
                                               volume, muted);
    }
    ResolveOrReject(promise, success, volume);
  }

  return promise.forget();
}

already_AddRefed<Promise> AudioChannelHandler::SetVolume(float aVolume,
                                                         ErrorResult& aRv) {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIGlobalObject> parentObject = do_QueryInterface(GetParentObject());
  RefPtr<Promise> promise = Promise::Create(parentObject, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  if (IsRemoteFrame()) {
    mBrowserParent->SendSetAudioChannelVolume(
        static_cast<uint32_t>(mAudioChannel), aVolume, false,
        GetDefaultIpcResolve(promise), GetDefaultIpcReject(promise));
  } else {
    bool success = false;
    RefPtr<AudioChannelService> service = AudioChannelService::GetOrCreate();
    if (service) {
      success = service->SetAudioChannelVolume(mFrameWindow, mAudioChannel,
                                               aVolume, false);
    }
    ResolveOrReject(promise, success);
  }

  return promise.forget();
}

already_AddRefed<Promise> AudioChannelHandler::GetSuspended(ErrorResult& aRv) {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIGlobalObject> parentObject = do_QueryInterface(GetParentObject());
  RefPtr<Promise> promise = Promise::Create(parentObject, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  if (IsRemoteFrame()) {
    auto resolve = [promise](std::tuple<bool, uint32_t>&& aResult) {
      bool success = std::get<0>(aResult);
      bool suspended = IsSuspended(std::get<1>(aResult));
      ResolveOrReject(promise, success, suspended);
    };
    mBrowserParent->SendGetAudioChannelSuspend(
        static_cast<uint32_t>(mAudioChannel), resolve,
        GetDefaultIpcReject(promise));
  } else {
    nsSuspendedTypes suspend = nsISuspendedTypes::NONE_SUSPENDED;
    bool success = false;
    RefPtr<AudioChannelService> service = AudioChannelService::GetOrCreate();
    if (service) {
      success =
          service->GetAudioChannelSuspend(mFrameWindow, mAudioChannel, suspend);
    }
    ResolveOrReject(promise, success, IsSuspended(suspend));
  }

  return promise.forget();
}

already_AddRefed<Promise> AudioChannelHandler::SetSuspended(bool aSuspended,
                                                            ErrorResult& aRv) {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIGlobalObject> parentObject = do_QueryInterface(GetParentObject());
  RefPtr<Promise> promise = Promise::Create(parentObject, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  nsSuspendedTypes suspendType = nsISuspendedTypes::NONE_SUSPENDED;
  if (aSuspended) {
    // If our channel is still playing, pause it. Otherwise reset it to default
    // suspend type.
    suspendType = mState == eStateActive
                      ? nsISuspendedTypes::SUSPENDED_PAUSE
                      : AudioChannelService::InitialSuspendType();
  }

  if (IsRemoteFrame()) {
    mBrowserParent->SendSetAudioChannelSuspend(
        static_cast<uint32_t>(mAudioChannel), suspendType,
        GetDefaultIpcResolve(promise), GetDefaultIpcReject(promise));
  } else {
    bool success = false;
    RefPtr<AudioChannelService> service = AudioChannelService::GetOrCreate();
    if (service) {
      success = service->SetAudioChannelSuspend(mFrameWindow, mAudioChannel,
                                                suspendType);
    }
    ResolveOrReject(promise, success);
  }

  return promise.forget();
}

already_AddRefed<Promise> AudioChannelHandler::IsActive(ErrorResult& aRv) {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIGlobalObject> parentObject = do_QueryInterface(GetParentObject());
  RefPtr<Promise> promise = Promise::Create(parentObject, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // If mState has been set, just return the cached value.
  if (mState != eStateUnknown) {
    bool active = mState == eStateActive;
    ResolveOrReject(promise, true, active);
  } else if (IsRemoteFrame()) {
    auto resolve = [promise](std::tuple<bool, bool>&& aResult) {
      bool success = std::get<0>(aResult);
      bool active = std::get<1>(aResult);
      ResolveOrReject(promise, success, active);
    };
    mBrowserParent->SendGetAudioChannelActivity(
        static_cast<uint32_t>(mAudioChannel), resolve,
        GetDefaultIpcReject(promise));
  } else {
    bool active = false;
    bool success = false;
    RefPtr<AudioChannelService> service = AudioChannelService::GetOrCreate();
    if (service) {
      active = service->IsAudioChannelActive(mFrameWindow, mAudioChannel);
      success = true;
    }
    ResolveOrReject(promise, success, active);
  }

  return promise.forget();
}

NS_IMETHODIMP
AudioChannelHandler::Observe(nsISupports* aSubject, const char* aTopic,
                             const char16_t* aData) {
  if (!aSubject) {
    return NS_OK;
  }

  nsAutoString name;
  AudioChannelService::GetAudioChannelString(mAudioChannel, name);

  nsAutoCString topic;
  topic.Assign("audiochannel-activity-");
  topic.Append(NS_ConvertUTF16toUTF8(name));

  if (strcmp(topic.get(), aTopic)) {
    return NS_OK;
  }

  if (IsRemoteFrame()) {
    // If our frame is in remote process, we expect the subject to be our
    // BrowserParent.
    nsISupports* isbp =
        NS_ISUPPORTS_CAST(nsIDOMEventListener*, mBrowserParent.get());
    if (isbp != aSubject) {
      return NS_OK;
    }
  } else {
    // If our frame is in the same process, we expect the subject to be the
    // ID of our frame window.
    nsCOMPtr<nsISupportsPRUint64> wrapper = do_QueryInterface(aSubject);
    if (!wrapper) {
      return NS_OK;
    }

    uint64_t windowID;
    nsresult rv = wrapper->GetData(&windowID);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (windowID != mFrameWindow->WindowID()) {
      return NS_OK;
    }
  }

  ProcessStateChanged(aData);
  return NS_OK;
}

void AudioChannelHandler::ProcessStateChanged(const char16_t* aData) {
  MOZ_LOG(
      AudioChannelService::GetAudioChannelLog(), LogLevel::Debug,
      ("AudioChannelHandler, ProcessStateChanged, this = %p, type = %" PRIu32
       ", state = %s\n",
       this, static_cast<uint32_t>(mAudioChannel),
       NS_ConvertUTF16toUTF8(aData).get()));

  nsAutoString value(aData);
  mState = value.EqualsASCII("active") ? eStateActive : eStateInactive;
  DispatchTrustedEvent(u"activestatechanged"_ns);
}

}  // namespace dom
}  // namespace mozilla
