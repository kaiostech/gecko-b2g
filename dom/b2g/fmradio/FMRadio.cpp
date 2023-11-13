/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FMRadio.h"
#include "AudioChannelService.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/ContentMediaController.h"
#include "mozilla/dom/FMRadioBinding.h"
#include "mozilla/dom/FMRadioService.h"
#include "mozilla/dom/MediaControlUtils.h"
#include "mozilla/dom/PFMRadioChild.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/HalTypes.h"
#include "mozilla/Preferences.h"
#include "nsCycleCollectionParticipant.h"

#undef LOG
#define LOG(args...) FM_LOG("FMRadio", args)

#undef MEDIACONTROL_LOG
#define MEDIACONTROL_LOG(msg, ...)           \
  MOZ_LOG(gMediaControlLog, LogLevel::Debug, \
          ("FMRadio=%p, " msg, this, ##__VA_ARGS__))

// The pref indicates if the device has an internal antenna.
// If the pref is true, the antanna will be always available.
#define DOM_FM_ANTENNA_INTERNAL_PREF "dom.fmradio.antenna.internal"

using mozilla::Preferences;

BEGIN_FMRADIO_NAMESPACE

class FMRadioRequest final : public FMRadioReplyRunnable {
 public:
  NS_DECL_ISUPPORTS_INHERITED

  static already_AddRefed<FMRadioRequest> Create(
      nsPIDOMWindowInner* aWindow, FMRadio* aFMRadio,
      FMRadioRequestArgs::Type aType = FMRadioRequestArgs::T__None) {
    MOZ_ASSERT(aType >= FMRadioRequestArgs::T__None &&
                   aType <= FMRadioRequestArgs::T__Last,
               "Wrong FMRadioRequestArgs in FMRadioRequest");

    ErrorResult rv;
    RefPtr<Promise> promise = Promise::Create(aWindow->AsGlobal(), rv);
    if (rv.Failed()) {
      return nullptr;
    }

    RefPtr<FMRadioRequest> request =
        new FMRadioRequest(promise, aFMRadio, aType);
    return request.forget();
  }

  NS_IMETHODIMP
  Run() override {
    MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

    nsCOMPtr<EventTarget> target = do_QueryReferent(mFMRadio);
    if (!target) {
      return NS_OK;
    }

    FMRadio* fmRadio = static_cast<FMRadio*>(static_cast<EventTarget*>(target));
    if (fmRadio->mIsShutdown) {
      return NS_OK;
    }

    if (!mPromise) {
      return NS_OK;
    }

    switch (mResponseType.type()) {
      case FMRadioResponseType::TErrorResponse:
        mPromise->MaybeRejectWithOperationError(
            NS_ConvertUTF16toUTF8(mResponseType.get_ErrorResponse().error()));
        break;
      case FMRadioResponseType::TSuccessResponse:
        mPromise->MaybeResolveWithUndefined();
        break;
      default:
        MOZ_CRASH();
    }
    mPromise = nullptr;
    return NS_OK;
  }

  already_AddRefed<Promise> GetPromise() {
    RefPtr<Promise> promise = mPromise.get();
    return promise.forget();
  }

 protected:
  FMRadioRequest(Promise* aPromise, FMRadio* aFMRadio,
                 FMRadioRequestArgs::Type aType)
      : mPromise(aPromise), mType(aType) {
    // |FMRadio| inherits from |nsIDOMEventTarget| and
    // |nsISupportsWeakReference| which both inherits from nsISupports, so
    // |nsISupports| is an ambiguous base of |FMRadio|, we have to cast
    // |aFMRadio| to one of the base classes.
    mFMRadio = do_GetWeakReference(static_cast<EventTarget*>(aFMRadio));
  }

  ~FMRadioRequest() {}

 private:
  RefPtr<Promise> mPromise;
  FMRadioRequestArgs::Type mType;
  nsWeakPtr mFMRadio;
};

NS_IMPL_ISUPPORTS_INHERITED0(FMRadioRequest, FMRadioReplyRunnable)

class FMRadio::MediaControlKeyListener final
    : public ContentMediaControlKeyReceiver {
 public:
  NS_INLINE_DECL_REFCOUNTING(MediaControlKeyListener, override)

  MOZ_INIT_OUTSIDE_CTOR explicit MediaControlKeyListener(FMRadio* aFMRadio) {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(aFMRadio);
    mFMRadio = do_GetWeakReference(static_cast<EventTarget*>(aFMRadio));
  }

  void Start() {
    MOZ_ASSERT(NS_IsMainThread());
    if (IsStarted()) {
      return;
    }

    if (!InitMediaAgent()) {
      MEDIACONTROL_LOG("Failed to start due to not able to init media agent!");
      return;
    }

    NotifyPlaybackStateChanged(MediaPlaybackState::eStarted);
  }

  void StopIfNeeded() {
    MOZ_ASSERT(NS_IsMainThread());
    if (!IsStarted()) {
      return;
    }
    NotifyMediaStoppedPlaying();
    NotifyPlaybackStateChanged(MediaPlaybackState::eStopped);

    // Remove ourselves from media agent, which would stop receiving event.
    mControlAgent->RemoveReceiver(this);
    mControlAgent = nullptr;
  }

  bool IsStarted() const { return mState != MediaPlaybackState::eStopped; }

  bool IsPlaying() const override {
    return Owner() && Owner()->IsPlayingThroughAudioChannel();
  }

  void NotifyMediaStartedPlaying() {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(IsStarted());
    if (mState == MediaPlaybackState::eStarted ||
        mState == MediaPlaybackState::ePaused) {
      NotifyPlaybackStateChanged(MediaPlaybackState::ePlayed);
      NotifyAudibleStateChanged(MediaAudibleState::eAudible);
    }
  }

  void NotifyMediaStoppedPlaying() {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(IsStarted());
    if (mState == MediaPlaybackState::ePlayed) {
      NotifyPlaybackStateChanged(MediaPlaybackState::ePaused);
      NotifyAudibleStateChanged(MediaAudibleState::eInaudible);
    }
  }

  void HandleMediaKey(MediaControlKey aKey) override {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(IsStarted());
    MEDIACONTROL_LOG("HandleEvent '%s'", ToMediaControlKeyStr(aKey));

    switch (aKey) {
      case MediaControlKey::Play: {
        RefPtr<Promise> p = Owner()->Enable(Owner()->mCachedFrequency);
        break;
      }
      case MediaControlKey::Pause: {
        RefPtr<Promise> p = Owner()->Disable();
        break;
      }
      case MediaControlKey::Stop: {
        RefPtr<Promise> p = Owner()->Disable();
        StopIfNeeded();
        break;
      }
      default:
        MOZ_ASSERT_UNREACHABLE("Unsupported key");
        break;
    }
  }

 private:
  ~MediaControlKeyListener() = default;

  BrowsingContext* GetCurrentBrowsingContext() const {
    if (!Owner()) {
      return nullptr;
    }
    nsPIDOMWindowInner* window = Owner()->GetParentObject();
    return window ? window->GetBrowsingContext() : nullptr;
  }

  bool InitMediaAgent() {
    MOZ_ASSERT(NS_IsMainThread());
    BrowsingContext* currentBC = GetCurrentBrowsingContext();
    mControlAgent = ContentMediaAgent::Get(currentBC);
    if (!mControlAgent) {
      return false;
    }
    MOZ_ASSERT(currentBC);
    mOwnerBrowsingContextId = currentBC->Id();
    MEDIACONTROL_LOG("Init agent in browsing context %" PRIu64,
                     mOwnerBrowsingContextId);
    mControlAgent->AddReceiver(this);
    return true;
  }

  FMRadio* Owner() const {
    nsCOMPtr<EventTarget> target = do_QueryReferent(mFMRadio);
    auto* fmRadio = static_cast<FMRadio*>(target.get());
    MOZ_ASSERT(fmRadio || !IsStarted());
    return fmRadio;
  }

  void NotifyPlaybackStateChanged(MediaPlaybackState aState) {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mControlAgent);
    MEDIACONTROL_LOG("NotifyMediaState from state='%s' to state='%s'",
                     ToMediaPlaybackStateStr(mState),
                     ToMediaPlaybackStateStr(aState));
    MOZ_ASSERT(mState != aState, "Should not notify same state again!");
    mState = aState;
    mControlAgent->NotifyMediaPlaybackChanged(mOwnerBrowsingContextId, mState);
  }

  void NotifyAudibleStateChanged(MediaAudibleState aState) {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(IsStarted());
    mControlAgent->NotifyMediaAudibleChanged(mOwnerBrowsingContextId, aState);
  }

  MediaPlaybackState mState = MediaPlaybackState::eStopped;
  nsWeakPtr mFMRadio;
  RefPtr<ContentMediaAgent> mControlAgent;
  uint64_t mOwnerBrowsingContextId;
};

FMRadio::FMRadio()
    : mRdsGroupMask(0),
      mCachedFrequency(0.0),
      mSuspendedByAudioChannel(false),
      mAudioChannelAgentEnabled(false),
      mHasInternalAntenna(false),
      mIsShutdown(false) {
  LOG("FMRadio is initialized.");
}

FMRadio::~FMRadio() {}

void FMRadio::Init(nsIGlobalObject* aGlobal) {
  BindToOwner(aGlobal);

  IFMRadioService::Singleton()->AddObserver(this);

  mHasInternalAntenna = Preferences::GetBool(DOM_FM_ANTENNA_INTERNAL_PREF,
                                             /* default = */ false);

  mAudioChannelAgent = new AudioChannelAgent();
  nsresult rv = mAudioChannelAgent->InitWithWeakCallback(
      GetOwner(), nsIAudioChannelAgent::AUDIO_AGENT_CHANNEL_CONTENT, this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    mAudioChannelAgent = nullptr;
    LOG("FMRadio::Init, Fail to initialize the audio channel agent");
    return;
  }

  mMediaControlKeyListener = new MediaControlKeyListener(this);
}

void FMRadio::Shutdown() {
  IFMRadioService::Singleton()->RemoveObserver(this);
  DisableAudioChannelAgent();
  mMediaControlKeyListener->StopIfNeeded();
  mMediaControlKeyListener = nullptr;
  mIsShutdown = true;
}

JSObject* FMRadio::WrapObject(JSContext* aCx,
                              JS::Handle<JSObject*> aGivenProto) {
  return FMRadio_Binding::Wrap(aCx, this, aGivenProto);
}

void FMRadio::Notify(const FMRadioEventType& aType) {
  switch (aType) {
    case FrequencyChanged:
      DispatchTrustedEvent(u"frequencychange"_ns);
      mCachedFrequency = IFMRadioService::Singleton()->GetFrequency();
      break;
    case EnabledChanged:
      if (Enabled()) {
        EnableAudioChannelAgent();
        DispatchTrustedEvent(u"enabled"_ns);
      } else {
        DisableAudioChannelAgent();
        DispatchTrustedEvent(u"disabled"_ns);
      }
      break;
    case RDSEnabledChanged:
      if (RdsEnabled()) {
        DispatchTrustedEvent(u"rdsenabled"_ns);
      } else {
        DispatchTrustedEvent(u"rdsdisabled"_ns);
      }
      break;
    case PIChanged:
      DispatchTrustedEvent(u"pichange"_ns);
      break;
    case PSChanged:
      DispatchTrustedEvent(u"pschange"_ns);
      break;
    case RadiotextChanged:
      DispatchTrustedEvent(u"rtchange"_ns);
      break;
    case PTYChanged:
      DispatchTrustedEvent(u"ptychange"_ns);
      break;
    case NewRDSGroup:
      DispatchTrustedEvent(u"newrdsgroup"_ns);
      break;
    default:
      MOZ_CRASH();
  }
}

/* static */
bool FMRadio::Enabled() { return IFMRadioService::Singleton()->IsEnabled(); }

bool FMRadio::RdsEnabled() {
  return IFMRadioService::Singleton()->IsRDSEnabled();
}

bool FMRadio::AntennaAvailable() const { return mHasInternalAntenna; }

Nullable<double> FMRadio::GetFrequency() const {
  return Enabled()
             ? Nullable<double>(IFMRadioService::Singleton()->GetFrequency())
             : Nullable<double>();
}

double FMRadio::FrequencyUpperBound() const {
  return IFMRadioService::Singleton()->GetFrequencyUpperBound();
}

double FMRadio::FrequencyLowerBound() const {
  return IFMRadioService::Singleton()->GetFrequencyLowerBound();
}

double FMRadio::ChannelWidth() const {
  return IFMRadioService::Singleton()->GetChannelWidth();
}

uint32_t FMRadio::RdsGroupMask() const { return mRdsGroupMask; }

void FMRadio::SetRdsGroupMask(uint32_t aRdsGroupMask) {
  mRdsGroupMask = aRdsGroupMask;
  IFMRadioService::Singleton()->SetRDSGroupMask(aRdsGroupMask);
}

Nullable<unsigned short> FMRadio::GetPi() const {
  return IFMRadioService::Singleton()->GetPi();
}

Nullable<uint8_t> FMRadio::GetPty() const {
  return IFMRadioService::Singleton()->GetPty();
}

void FMRadio::GetPs(DOMString& aPsname) const {
  if (!IFMRadioService::Singleton()->GetPs(aPsname)) {
    aPsname.SetNull();
  }
}

void FMRadio::GetRt(DOMString& aRadiotext) const {
  if (!IFMRadioService::Singleton()->GetRt(aRadiotext)) {
    aRadiotext.SetNull();
  }
}

void FMRadio::GetRdsgroup(JSContext* cx, JS::MutableHandle<JSObject*> retval) {
  uint64_t group;
  if (!IFMRadioService::Singleton()->GetRdsgroup(group)) {
    return;
  }

  ErrorResult rv;
  JSObject* rdsgroup = Uint16Array::Create(cx, this, 4, rv);
  if (rv.Failed()) {
    return;
  }

  JS::AutoCheckCannotGC nogc;
  bool isShared = false;
  uint16_t* data = JS_GetUint16ArrayData(rdsgroup, &isShared, nogc);
  MOZ_ASSERT(!isShared);  // Because created above.
  data[3] = group & 0xFFFF;
  group >>= 16;
  data[2] = group & 0xFFFF;
  group >>= 16;
  data[1] = group & 0xFFFF;
  group >>= 16;
  data[0] = group & 0xFFFF;

  JS::ExposeObjectToActiveJS(rdsgroup);
  retval.set(rdsgroup);
}

already_AddRefed<Promise> FMRadio::Enable(double aFrequency) {
  nsCOMPtr<nsPIDOMWindowInner> win = GetOwner();
  if (!win) {
    return nullptr;
  }

  RefPtr<FMRadioRequest> r =
      FMRadioRequest::Create(win, this, FMRadioRequestArgs::TEnableRequestArgs);
  if (!r) {
    return nullptr;
  }

  mMediaControlKeyListener->Start();
  IFMRadioService::Singleton()->Enable(aFrequency, r);
  return r->GetPromise();
}

already_AddRefed<Promise> FMRadio::Disable() {
  nsCOMPtr<nsPIDOMWindowInner> win = GetOwner();
  if (!win) {
    return nullptr;
  }

  RefPtr<FMRadioRequest> r = FMRadioRequest::Create(
      win, this, FMRadioRequestArgs::TDisableRequestArgs);
  if (!r) {
    return nullptr;
  }

  IFMRadioService::Singleton()->Disable(r);
  return r->GetPromise();
}

already_AddRefed<Promise> FMRadio::SetFrequency(double aFrequency) {
  nsCOMPtr<nsPIDOMWindowInner> win = GetOwner();
  if (!win) {
    return nullptr;
  }

  RefPtr<FMRadioRequest> r = FMRadioRequest::Create(win, this);
  if (!r) {
    return nullptr;
  }

  IFMRadioService::Singleton()->SetFrequency(aFrequency, r);
  return r->GetPromise();
}

already_AddRefed<Promise> FMRadio::SeekUp() {
  nsCOMPtr<nsPIDOMWindowInner> win = GetOwner();
  if (!win) {
    return nullptr;
  }

  RefPtr<FMRadioRequest> r = FMRadioRequest::Create(win, this);
  if (!r) {
    return nullptr;
  }

  IFMRadioService::Singleton()->Seek(hal::FM_RADIO_SEEK_DIRECTION_UP, r);
  return r->GetPromise();
}

already_AddRefed<Promise> FMRadio::SeekDown() {
  nsCOMPtr<nsPIDOMWindowInner> win = GetOwner();
  if (!win) {
    return nullptr;
  }

  RefPtr<FMRadioRequest> r = FMRadioRequest::Create(win, this);
  if (!r) {
    return nullptr;
  }

  IFMRadioService::Singleton()->Seek(hal::FM_RADIO_SEEK_DIRECTION_DOWN, r);
  return r->GetPromise();
}

already_AddRefed<Promise> FMRadio::CancelSeek() {
  nsCOMPtr<nsPIDOMWindowInner> win = GetOwner();
  if (!win) {
    return nullptr;
  }

  RefPtr<FMRadioRequest> r = FMRadioRequest::Create(win, this);
  if (!r) {
    return nullptr;
  }

  IFMRadioService::Singleton()->CancelSeek(r);
  return r->GetPromise();
}

already_AddRefed<Promise> FMRadio::EnableRDS() {
  nsCOMPtr<nsPIDOMWindowInner> win = GetOwner();
  if (!win) {
    return nullptr;
  }

  RefPtr<FMRadioRequest> r = FMRadioRequest::Create(win, this);
  if (!r) {
    return nullptr;
  }

  IFMRadioService::Singleton()->EnableRDS(r);
  return r->GetPromise();
}

already_AddRefed<Promise> FMRadio::DisableRDS() {
  nsCOMPtr<nsPIDOMWindowInner> win = GetOwner();
  if (!win) {
    return nullptr;
  }

  RefPtr<FMRadioRequest> r = FMRadioRequest::Create(win, this);
  if (!r) {
    return nullptr;
  }

  IFMRadioService::Singleton()->DisableRDS(r);
  return r->GetPromise();
}

void FMRadio::EnableAudioChannelAgent() {
  NS_ENSURE_TRUE_VOID(mAudioChannelAgent);
  mAudioChannelAgent->NotifyStartedPlaying(
      AudioChannelService::AudibleState::eAudible);
  mAudioChannelAgent->PullInitialUpdate();
  mAudioChannelAgentEnabled = true;
}

void FMRadio::DisableAudioChannelAgent() {
  if (mAudioChannelAgentEnabled) {
    NS_ENSURE_TRUE_VOID(mAudioChannelAgent);
    mAudioChannelAgent->NotifyStoppedPlaying();
    mAudioChannelAgentEnabled = false;
    mSuspendedByAudioChannel = false;
    mMediaControlKeyListener->NotifyMediaStoppedPlaying();
  }
}

bool FMRadio::IsPlayingThroughAudioChannel() {
  return mAudioChannelAgentEnabled && !mSuspendedByAudioChannel;
}

NS_IMETHODIMP FMRadio::WindowVolumeChanged(float aVolume, bool aMuted) {
  IFMRadioService::Singleton()->SetVolume(aMuted ? 0.0f : aVolume);
  return NS_OK;
}

NS_IMETHODIMP FMRadio::WindowSuspendChanged(SuspendTypes aSuspend) {
  mSuspendedByAudioChannel = aSuspend != nsISuspendedTypes::NONE_SUSPENDED;
  if (mSuspendedByAudioChannel) {
    mMediaControlKeyListener->NotifyMediaStoppedPlaying();
  } else {
    mMediaControlKeyListener->NotifyMediaStartedPlaying();
  }
  IFMRadioService::Singleton()->EnableAudio(!mSuspendedByAudioChannel);
  return NS_OK;
}

NS_IMETHODIMP FMRadio::WindowAudioCaptureChanged(bool aCapture) {
  return NS_OK;
}

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(FMRadio)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsIAudioChannelAgentCallback)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(FMRadio, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(FMRadio, DOMEventTargetHelper)

END_FMRADIO_NAMESPACE
