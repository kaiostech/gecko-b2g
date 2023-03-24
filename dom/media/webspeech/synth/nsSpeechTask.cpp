/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioChannelService.h"
#include "AudioSegment.h"
#include "nsSpeechTask.h"
#include "nsSynthVoiceRegistry.h"
#include "nsXULAppAPI.h"
#include "SharedBuffer.h"
#include "SpeechSynthesis.h"
#include "MediaTrackListener.h"

#undef LOG
extern mozilla::LogModule* GetSpeechSynthLog();
#define LOG(type, msg) MOZ_LOG(GetSpeechSynthLog(), type, msg)

#define AUDIO_TRACK 1

namespace mozilla::dom {

class SynthStreamListener : public MediaTrackListener {
public:
  explicit SynthStreamListener(nsSpeechTask* aSpeechTask,
                               MediaTrack* aStream) :
    mSpeechTask(aSpeechTask),
    mStream(aStream) {}

  void DoNotifyStarted() {
    if (mSpeechTask && !mStream->IsDestroyed()) {
      mSpeechTask->DispatchStart();
    }
  }

  void DoNotifyFinished() {
    if (mSpeechTask && !mStream->IsDestroyed()) {
      mSpeechTask->DispatchEnd(mSpeechTask->GetCurrentTime(),
                               mSpeechTask->GetCurrentCharOffset());
    }
  }

  void NotifyEnded(MediaTrackGraph* aGraph) override {
    aGraph->DispatchToMainThreadStableState(NS_NewRunnableFunction(
      "NotifyEnded endRunnable",
      [self = RefPtr<SynthStreamListener>(this)] {
        if (self) {
          self->DoNotifyFinished();
        }
      }));
  }

  void NotifyRemoved(MediaTrackGraph* aGraph) override {
    mSpeechTask = nullptr;
    // Dereference MediaTrack to destroy safety
    mStream = nullptr;
  }

private:
  // Raw pointer; if we exist, the stream exists,
  // and 'mSpeechTask' exclusively owns it and therefor exists as well.
  nsSpeechTask* mSpeechTask;
  // This is KungFuDeathGrip for MediaTrack
  RefPtr<MediaTrack> mStream;
};

// nsSpeechTask

NS_IMPL_CYCLE_COLLECTION_WEAK(nsSpeechTask, mSpeechSynthesis, mUtterance,
                              mCallback)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsSpeechTask)
  NS_INTERFACE_MAP_ENTRY(nsISpeechTask)
  NS_INTERFACE_MAP_ENTRY(nsIAudioChannelAgentCallback)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISpeechTask)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsSpeechTask)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsSpeechTask)

nsSpeechTask::nsSpeechTask(SpeechSynthesisUtterance* aUtterance,
                           bool aShouldResistFingerprinting)
    : mUtterance(aUtterance),
      mInited(false),
      mPrePaused(false),
      mPreCanceled(false),
      mCallback(nullptr),
      mShouldResistFingerprinting(aShouldResistFingerprinting),
      mState(STATE_PENDING) {
  mText = aUtterance->mText;
  mVolume = aUtterance->Volume();
}

nsSpeechTask::nsSpeechTask(float aVolume, const nsAString& aText,
                           bool aShouldResistFingerprinting)
    : mUtterance(nullptr),
      mVolume(aVolume),
      mText(aText),
      mInited(false),
      mPrePaused(false),
      mPreCanceled(false),
      mCallback(nullptr),
      mShouldResistFingerprinting(aShouldResistFingerprinting),
      mState(STATE_PENDING) {}

nsSpeechTask::~nsSpeechTask() {
  LOG(LogLevel::Debug, ("~nsSpeechTask"));
  if (mStream) {
    if (!mStream->IsDestroyed()) {
      mStream->Destroy();
    }

    // This will finally destroyed by SynthStreamListener becasue
    // MediaStream::Destroy() is async.
    mStream = nullptr;
  }
}

void nsSpeechTask::Init() { mInited = true; }

void nsSpeechTask::SetChosenVoiceURI(const nsAString& aUri) {
  mChosenVoiceURI = aUri;
}

NS_IMETHODIMP
nsSpeechTask::Setup(nsISpeechTaskCallback* aCallback) {
  MOZ_ASSERT(XRE_IsParentProcess());

  LOG(LogLevel::Debug, ("nsSpeechTask::Setup"));

  mCallback = aCallback;

  return NS_OK;
}

NS_IMETHODIMP
nsSpeechTask::SetupAudioNative(nsISpeechTaskCallback* aCallback, uint32_t aRate) {
  MOZ_ASSERT(XRE_IsParentProcess());

  MediaTrackGraph* g1 = MediaTrackGraph::GetInstance(
      MediaTrackGraph::AUDIO_THREAD_DRIVER, /*window*/ nullptr, aRate,
      /*OutputDeviceID*/ nullptr, mozilla::dom::AudioChannel::Normal);
  mStream = g1->CreateSourceTrack(MediaSegment::AUDIO);

  MOZ_ASSERT(mStream);
  SynthStreamListener* listener = new SynthStreamListener(this, mStream);
  mStream->AddListener(listener);
  mStream->AddAudioOutput(this);
  mStream->SetAudioOutputVolume(this, mVolume);
  listener->DoNotifyStarted();
  mCallback = aCallback;

  return NS_OK;
}

static RefPtr<mozilla::SharedBuffer>
makeSamples(int16_t* aData, uint32_t aDataLen) {
  CheckedInt<size_t> size = aDataLen * sizeof(int16_t);
  RefPtr<mozilla::SharedBuffer> samples = SharedBuffer::Create(size);
  int16_t* frames = static_cast<int16_t*>(samples->Data());

  for (uint32_t i = 0; i < aDataLen; i++) {
    frames[i] = aData[i];
  }
  return samples;
}

NS_IMETHODIMP
nsSpeechTask::SendAudioNative(int16_t* aData, uint32_t aDataLen) {
  MOZ_ASSERT(XRE_IsParentProcess());

  if(NS_WARN_IF(!(mStream))) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  if(NS_WARN_IF(mStream->IsDestroyed())) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  RefPtr<mozilla::SharedBuffer> samples = makeSamples(aData, aDataLen);
  SendAudioImpl(samples, aDataLen);

  return NS_OK;
}

void
nsSpeechTask::SendAudioImpl(RefPtr<mozilla::SharedBuffer>& aSamples, uint32_t aDataLen) {
  if (aDataLen == 0) {
    mStream->End();
    return;
  }

  AudioSegment segment;
  AutoTArray<const int16_t*, 1> channelData;
  channelData.AppendElement(static_cast<int16_t*>(aSamples->Data()));
  segment.AppendFrames(aSamples.forget(), channelData, aDataLen,
                       PRINCIPAL_HANDLE_NONE);
  mStream->AppendData(&segment);
}

NS_IMETHODIMP
nsSpeechTask::DispatchStart() {
  nsSynthVoiceRegistry::GetInstance()->SetIsSpeaking(true);
  return DispatchStartImpl();
}

nsresult nsSpeechTask::DispatchStartImpl() {
  return DispatchStartImpl(mChosenVoiceURI);
}

nsresult nsSpeechTask::DispatchStartImpl(const nsAString& aUri) {
  LOG(LogLevel::Debug, ("nsSpeechTask::DispatchStartImpl"));

  MOZ_ASSERT(mUtterance);
  if (NS_WARN_IF(mState != STATE_PENDING)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  CreateAudioChannelAgent();

  mState = STATE_SPEAKING;
  mUtterance->mChosenVoiceURI = aUri;
  mUtterance->DispatchSpeechSynthesisEvent(u"start"_ns, 0, nullptr, 0, u""_ns);

  return NS_OK;
}

NS_IMETHODIMP
nsSpeechTask::DispatchEnd(float aElapsedTime, uint32_t aCharIndex) {
  // After we end, no callback functions should go through.
  mCallback = nullptr;

  if (!mPreCanceled) {
    nsSynthVoiceRegistry::GetInstance()->SpeakNext();
  }

  return DispatchEndImpl(aElapsedTime, aCharIndex);
}

nsresult nsSpeechTask::DispatchEndImpl(float aElapsedTime,
                                       uint32_t aCharIndex) {
  LOG(LogLevel::Debug, ("nsSpeechTask::DispatchEndImpl"));

  DestroyAudioChannelAgent();

  MOZ_ASSERT(mUtterance);
  if (NS_WARN_IF(mState == STATE_ENDED)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  RefPtr<SpeechSynthesisUtterance> utterance = mUtterance;

  if (mSpeechSynthesis) {
    mSpeechSynthesis->OnEnd(this);
  }

  mState = STATE_ENDED;
  utterance->DispatchSpeechSynthesisEvent(u"end"_ns, aCharIndex, nullptr,
                                          aElapsedTime, u""_ns);

  return NS_OK;
}

NS_IMETHODIMP
nsSpeechTask::DispatchPause(float aElapsedTime, uint32_t aCharIndex) {
  return DispatchPauseImpl(aElapsedTime, aCharIndex);
}

nsresult nsSpeechTask::DispatchPauseImpl(float aElapsedTime,
                                         uint32_t aCharIndex) {
  LOG(LogLevel::Debug, ("nsSpeechTask::DispatchPauseImpl"));
  MOZ_ASSERT(mUtterance);
  if (NS_WARN_IF(mUtterance->mPaused)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  if (NS_WARN_IF(mState == STATE_ENDED)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  mUtterance->mPaused = true;
  if (mState == STATE_SPEAKING) {
    mUtterance->DispatchSpeechSynthesisEvent(u"pause"_ns, aCharIndex, nullptr,
                                             aElapsedTime, u""_ns);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSpeechTask::DispatchResume(float aElapsedTime, uint32_t aCharIndex) {
  return DispatchResumeImpl(aElapsedTime, aCharIndex);
}

nsresult nsSpeechTask::DispatchResumeImpl(float aElapsedTime,
                                          uint32_t aCharIndex) {
  LOG(LogLevel::Debug, ("nsSpeechTask::DispatchResumeImpl"));
  MOZ_ASSERT(mUtterance);
  if (NS_WARN_IF(!(mUtterance->mPaused))) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  if (NS_WARN_IF(mState == STATE_ENDED)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  mUtterance->mPaused = false;
  if (mState == STATE_SPEAKING) {
    mUtterance->DispatchSpeechSynthesisEvent(u"resume"_ns, aCharIndex, nullptr,
                                             aElapsedTime, u""_ns);
  }

  return NS_OK;
}

void nsSpeechTask::ForceError(float aElapsedTime, uint32_t aCharIndex) {
  DispatchError(aElapsedTime, aCharIndex);
}

NS_IMETHODIMP
nsSpeechTask::DispatchError(float aElapsedTime, uint32_t aCharIndex) {
  if (!mPreCanceled) {
    nsSynthVoiceRegistry::GetInstance()->SpeakNext();
  }

  return DispatchErrorImpl(aElapsedTime, aCharIndex);
}

nsresult nsSpeechTask::DispatchErrorImpl(float aElapsedTime,
                                         uint32_t aCharIndex) {
  LOG(LogLevel::Debug, ("nsSpeechTask::DispatchErrorImpl"));

  DestroyAudioChannelAgent();

  MOZ_ASSERT(mUtterance);
  if (NS_WARN_IF(mState == STATE_ENDED)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (mSpeechSynthesis) {
    mSpeechSynthesis->OnEnd(this);
  }

  mState = STATE_ENDED;
  mUtterance->DispatchSpeechSynthesisEvent(u"error"_ns, aCharIndex, nullptr,
                                           aElapsedTime, u""_ns);
  return NS_OK;
}

NS_IMETHODIMP
nsSpeechTask::DispatchBoundary(const nsAString& aName, float aElapsedTime,
                               uint32_t aCharIndex, uint32_t aCharLength,
                               uint8_t argc) {
  return DispatchBoundaryImpl(aName, aElapsedTime, aCharIndex, aCharLength,
                              argc);
}

nsresult nsSpeechTask::DispatchBoundaryImpl(const nsAString& aName,
                                            float aElapsedTime,
                                            uint32_t aCharIndex,
                                            uint32_t aCharLength,
                                            uint8_t argc) {
  MOZ_ASSERT(mUtterance);
  if (NS_WARN_IF(mState != STATE_SPEAKING)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  mUtterance->DispatchSpeechSynthesisEvent(
      u"boundary"_ns, aCharIndex,
      argc ? static_cast<Nullable<uint32_t> >(aCharLength) : nullptr,
      aElapsedTime, aName);

  return NS_OK;
}

NS_IMETHODIMP
nsSpeechTask::DispatchMark(const nsAString& aName, float aElapsedTime,
                           uint32_t aCharIndex) {
  return DispatchMarkImpl(aName, aElapsedTime, aCharIndex);
}

nsresult nsSpeechTask::DispatchMarkImpl(const nsAString& aName,
                                        float aElapsedTime,
                                        uint32_t aCharIndex) {
  MOZ_ASSERT(mUtterance);
  if (NS_WARN_IF(mState != STATE_SPEAKING)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  mUtterance->DispatchSpeechSynthesisEvent(u"mark"_ns, aCharIndex, nullptr,
                                           aElapsedTime, aName);
  return NS_OK;
}

void nsSpeechTask::Pause() {
  MOZ_ASSERT(XRE_IsParentProcess());

  if (mCallback) {
    DebugOnly<nsresult> rv = mCallback->OnPause();
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Unable to call onPause() callback");
  }

  if (!mInited) {
    mPrePaused = true;
  }
}

void nsSpeechTask::Resume() {
  MOZ_ASSERT(XRE_IsParentProcess());

  if (mCallback) {
    DebugOnly<nsresult> rv = mCallback->OnResume();
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "Unable to call onResume() callback");
  }

  if (mPrePaused) {
    mPrePaused = false;
    nsSynthVoiceRegistry::GetInstance()->ResumeQueue();
  }
}

void nsSpeechTask::Cancel() {
  MOZ_ASSERT(XRE_IsParentProcess());

  LOG(LogLevel::Debug, ("nsSpeechTask::Cancel"));

  if (mCallback) {
    DebugOnly<nsresult> rv = mCallback->OnCancel();
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "Unable to call onCancel() callback");
  }

  if (!mInited) {
    mPreCanceled = true;
  }

  if (mStream) {
    mStream->Suspend();
    DispatchEnd(GetCurrentTime(), GetCurrentCharOffset());
  }
}

void nsSpeechTask::ForceEnd() {
  if (mStream) {
    mStream->Suspend();
  }

  if (!mInited) {
    mPreCanceled = true;
  }

  DispatchEnd(0, 0);
}

float
nsSpeechTask::GetCurrentTime() {
  return mStream ? (float)(mStream->GetCurrentTime() / 1000000.0) : 0;
}

uint32_t
nsSpeechTask::GetCurrentCharOffset() {
  return mStream && mStream->IsEnded() ? mText.Length() : 0;
}

void nsSpeechTask::SetSpeechSynthesis(SpeechSynthesis* aSpeechSynthesis) {
  mSpeechSynthesis = aSpeechSynthesis;
}

void nsSpeechTask::CreateAudioChannelAgent() {
  if (!mUtterance) {
    return;
  }

  if (mAudioChannelAgent) {
    mAudioChannelAgent->NotifyStoppedPlaying();
  }

  mAudioChannelAgent = new AudioChannelAgent();
  mAudioChannelAgent->InitWithWeakCallback(
      mUtterance->GetOwner(),
      static_cast<int32_t>(AudioChannelService::GetDefaultAudioChannel()),
      this);

  nsresult rv = mAudioChannelAgent->NotifyStartedPlaying(
      AudioChannelService::AudibleState::eAudible);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  mAudioChannelAgent->PullInitialUpdate();
}

void nsSpeechTask::DestroyAudioChannelAgent() {
  if (mAudioChannelAgent) {
    mAudioChannelAgent->NotifyStoppedPlaying();
    mAudioChannelAgent = nullptr;
  }
}

NS_IMETHODIMP
nsSpeechTask::WindowVolumeChanged(float aVolume, bool aMuted) {
  SetAudioOutputVolume(aMuted ? 0.0 : mVolume * aVolume);
  return NS_OK;
}

NS_IMETHODIMP
nsSpeechTask::WindowSuspendChanged(nsSuspendedTypes aSuspend) {
  if (!mUtterance) {
    return NS_OK;
  }

  if (aSuspend == nsISuspendedTypes::NONE_SUSPENDED && mUtterance->mPaused) {
    Resume();
  } else if (aSuspend != nsISuspendedTypes::NONE_SUSPENDED &&
             !mUtterance->mPaused) {
    Pause();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSpeechTask::WindowAudioCaptureChanged(bool aCapture) {
  // This is not supported yet.
  return NS_OK;
}

void nsSpeechTask::SetAudioOutputVolume(float aVolume) {
  if (mCallback) {
    mCallback->OnVolumeChanged(aVolume);
  }
}

}  // namespace mozilla::dom
