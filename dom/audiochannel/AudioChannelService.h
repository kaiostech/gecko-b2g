/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_audiochannelservice_h__
#define mozilla_dom_audiochannelservice_h__

#include "nsIObserver.h"
#include "nsTObserverArray.h"
#include "nsTArray.h"

#include "AudioChannelAgent.h"
#include "nsAttrValue.h"
#include "mozilla/dom/AudioChannelBinding.h"
#include "mozilla/Logging.h"
#include "mozilla/UniquePtr.h"

#include <functional>

class nsIRunnable;
class nsPIDOMWindowOuter;
struct PRLogModuleInfo;

namespace mozilla::dom {

#ifdef MOZ_WIDGET_GONK
class SpeakerManagerService;
#endif

class BrowserParent;

#define NUMBER_OF_AUDIO_CHANNELS (uint32_t) AudioChannel::EndGuard_

class AudioPlaybackConfig {
 public:
  AudioPlaybackConfig()
      : mVolume(1.0),
        mMuted(false),
        mSuspend(nsISuspendedTypes::NONE_SUSPENDED) {}

  AudioPlaybackConfig(float aVolume, bool aMuted, uint32_t aSuspended)
      : mVolume(aVolume), mMuted(aMuted), mSuspend(aSuspended) {}

  void SetConfig(float aVolume, bool aMuted, uint32_t aSuspended) {
    mVolume = aVolume;
    mMuted = aMuted;
    mSuspend = aSuspended;
  }

  float mVolume;
  bool mMuted;
  uint32_t mSuspend;
  bool mCapturedAudio = false;
};

class AudioChannelService final : public nsIObserver {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  /**
   * We use `AudibleState` to represent the audible state of an owner of audio
   * channel agent. Those information in AudioChannelWindow could help us to
   * determine if a tab is being audible or not, in order to tell Chrome JS to
   * show the sound indicator or delayed autoplay icon on the tab bar.
   *
   * - Sound indicator
   * When a tab is playing sound, we would show the sound indicator on tab bar
   * to tell users that this tab is producing sound now. In addition, the sound
   * indicator also give users an ablility to mute or unmute tab.
   *
   * When an AudioChannelWindow first contains an agent with state `eAudible`,
   * or an AudioChannelWindow losts its last agent with state `eAudible`, we
   * would notify Chrome JS about those changes, to tell them that a tab has
   * been being audible or not, in order to display or remove the indicator for
   * a corresponding tab.
   *
   * - Delayed autoplay icon (Play Tab icon)
   * When we enable delaying autoplay, which is to postpone the autoplay media
   * for unvisited tab until it first goes to foreground, or user click the
   * play tab icon to resume the delayed media.
   *
   * When an AudioChannelWindow first contains an agent with state `eAudible` or
   * `eMaybeAudible`, we would notify Chrome JS about this change, in order to
   * show the delayed autoplay tab icon to user, which is used to notice user
   * there is a media being delayed starting, and then user can click the play
   * tab icon to resume the start of media, or visit that tab to resume delayed
   * media automatically.
   *
   * According to our UX design, we don't show this icon for inaudible media.
   * The reason of showing the icon for a tab, where the agent starts with state
   * `eMaybeAudible`, is because some video might be silent in the beginning
   * but would soon become audible later.
   *
   * ---------------------------------------------------------------------------
   *
   * eNotAudible : agent is not audible
   * eMaybeAudible : agent is not audible now, but it might be audible later
   * eAudible : agent is audible now
   */
  enum AudibleState : uint8_t {
    eNotAudible = 0,
    eMaybeAudible = 1,
    eAudible = 2
  };

  enum AudioCaptureState : bool { eCapturing = true, eNotCapturing = false };

  enum AudibleChangedReasons : uint32_t {
    eVolumeChanged = 0,
    eDataAudibleChanged = 1,
    ePauseStateChanged = 2
  };

  /**
   * Returns the AudioChannelServce singleton.
   * If AudioChannelService doesn't exist, create and return new one.
   * Only to be called from main thread.
   */
  static already_AddRefed<AudioChannelService> GetOrCreate();

  /**
   * Returns the AudioChannelService singleton if one exists.
   * If AudioChannelService doesn't exist, returns null.
   */
  static already_AddRefed<AudioChannelService> Get();

  static nsSuspendedTypes InitialSuspendType();

  static LogModule* GetAudioChannelLog();

  static bool IsEnableAudioCompeting();

  static already_AddRefed<nsPIDOMWindowOuter> GetTopAppWindow(
      nsPIDOMWindowOuter* aWindow);

  static already_AddRefed<nsPIDOMWindowOuter> GetTopAppWindow(
      BrowserParent* aBrowserParent);

  /**
   * Any audio channel agent that starts playing should register itself to
   * this service, sharing the AudioChannel.
   */
  void RegisterAudioChannelAgent(AudioChannelAgent* aAgent,
                                 AudibleState aAudible);

  /**
   * Any audio channel agent that stops playing should unregister itself to
   * this service.
   */
  void UnregisterAudioChannelAgent(AudioChannelAgent* aAgent);

  /**
   * For nested iframes.
   */
  void RegisterBrowserParent(BrowserParent* aBrowserParent);
  void UnregisterBrowserParent(BrowserParent* aBrowserParent);

  /**
   * Return the state to indicate this audioChannel for his window should keep
   * playing/muted/suspended.
   */
  AudioPlaybackConfig GetMediaConfig(nsPIDOMWindowOuter* aWindow,
                                     uint32_t aAudioChannel) const;

  /**
   * Called this method when the audible state of the audio playback changed,
   * it would dispatch the playback event to observers which want to know the
   * actual audible state of the window.
   */
  void AudioAudibleChanged(AudioChannelAgent* aAgent, AudibleState aAudible,
                           AudibleChangedReasons aReason);

  /* Methods for the AudioChannelHandler */
  bool GetAudioChannelVolume(nsPIDOMWindowOuter* aWindow, AudioChannel aChannel,
                             float& aVolume, bool& aMuted);

  bool SetAudioChannelVolume(nsPIDOMWindowOuter* aWindow, AudioChannel aChannel,
                             float aVolume, bool aMuted);

  bool GetAudioChannelSuspend(nsPIDOMWindowOuter* aWindow,
                              AudioChannel aChannel,
                              nsSuspendedTypes& aSuspend);

  bool SetAudioChannelSuspend(nsPIDOMWindowOuter* aWindow,
                              AudioChannel aChannel, nsSuspendedTypes aSuspend);

  bool IsAudioChannelActive(nsPIDOMWindowOuter* aWindow, AudioChannel aChannel);

  bool IsWindowActive(nsPIDOMWindowOuter* aWindow);

  /**
   * Return true if there is a telephony channel active in this process
   * or one of its subprocesses.
   */
  bool TelephonyChannelIsActive();

  /**
   * Return true if a normal or content channel is active for the given
   * process ID.
   */
  bool ProcessContentOrNormalChannelIsActive(uint64_t aChildID);

  /***
   * AudioChannelManager calls this function to notify the default channel used
   * to adjust volume when there is no any active channel. if aChannel is -1,
   * the default audio channel will be used. Otherwise aChannel is casted to
   * AudioChannel enum.
   */
  virtual void SetDefaultVolumeControlChannel(int32_t aChannel, bool aVisible);

  bool AnyAudioChannelIsActive(bool aCheckChild = true);

  void RefreshAgentsVolume(nsPIDOMWindowOuter* aWindow, AudioChannel aChannel,
                           float aVolume, bool aMuted);

  void RefreshAgentsVolumeAndPropagate(nsPIDOMWindowOuter* aWindow,
                                       AudioChannel aAudioChannel,
                                       float aVolume, bool aMuted);

  void RefreshAgentsSuspendAndPropagate(nsPIDOMWindowOuter* aWindow,
                                        AudioChannel aAudioChannel,
                                        nsSuspendedTypes aSuspend);

  // This method needs to know the inner window that wants to capture audio. We
  // group agents per top outer window, but we can have multiple innerWindow per
  // top outerWindow (subiframes, etc.) and we have to identify all the agents
  // just for a particular innerWindow.
  void SetWindowAudioCaptured(nsPIDOMWindowOuter* aWindow,
                              uint64_t aInnerWindowID, bool aCapture);

#ifdef MOZ_WIDGET_GONK
  void RegisterSpeakerManager(SpeakerManagerService* aService) {
    if (!mSpeakerManagerServices.Contains(aService)) {
      mSpeakerManagerServices.AppendElement(aService);
    }
  }

  void UnregisterSpeakerManager(SpeakerManagerService* aService) {
    mSpeakerManagerServices.RemoveElement(aService);
  }
#endif

  static const nsAttrValue::EnumTable* GetAudioChannelTable();
  static AudioChannel GetAudioChannel(const nsAString& aString);
  static AudioChannel GetDefaultAudioChannel();
  static void GetAudioChannelString(AudioChannel aChannel, nsAString& aString);
  static void GetDefaultAudioChannelString(nsAString& aString);

  void Notify(uint64_t aWindowID);

  void ChildStatusReceived(uint64_t aChildID, bool aTelephonyChannel,
                           bool aContentOrNormalChannel, bool aAnyChannel);

  void NotifyResumingDelayedMedia(nsPIDOMWindowOuter* aWindow);

 private:
  AudioChannelService();
  ~AudioChannelService();

  void RefreshAgents(nsPIDOMWindowOuter* aWindow,
                     const std::function<void(AudioChannelAgent*)>& aFunc);

  void RefreshAgentsSuspend(nsPIDOMWindowOuter* aWindow,
                            AudioChannel aChannel,
                            nsSuspendedTypes aSuspend);

  static void CreateServiceIfNeeded();

  /**
   * Shutdown the singleton.
   */
  static void Shutdown();

  void MaybeSendStatusUpdate();

  bool ContentOrNormalChannelIsActive();

  /* Send the default-volume-channel-changed notification */
  void SetDefaultVolumeControlChannelInternal(int32_t aChannel, bool aVisible,
                                              uint64_t aChildID);

  void RefreshAgentsAudioFocusChanged(AudioChannelAgent* aAgent);

  class AudioChannelConfig final : public AudioPlaybackConfig {
   public:
    AudioChannelConfig()
        : AudioPlaybackConfig(1.0, false, InitialSuspendType()),
          mNumberOfAgents(0) {}

    uint32_t mNumberOfAgents;
  };

  class AudioChannelWindow final {
   public:
    explicit AudioChannelWindow(uint64_t aWindowID)
        : mWindowID(aWindowID),
          mIsAudioCaptured(false),
          mShouldSendActiveMediaBlockStopEvent(false) {}

    void AudioAudibleChanged(AudioChannelAgent* aAgent, AudibleState aAudible,
                             AudibleChangedReasons aReason);

    void AppendAgent(AudioChannelAgent* aAgent, AudibleState aAudible);
    void RemoveAgent(AudioChannelAgent* aAgent);

    void NotifyMediaBlockStop(nsPIDOMWindowOuter* aWindow);

    bool IsChannelActive(AudioChannel aChannel);

    uint64_t mWindowID;
    bool mIsAudioCaptured;
    AudioChannelConfig mChannels[NUMBER_OF_AUDIO_CHANNELS];

    // Raw pointer because the AudioChannelAgent must unregister itself.
    nsTObserverArray<AudioChannelAgent*> mAgents;
    nsTObserverArray<AudioChannelAgent*> mAudibleAgents;

    // If we've dispatched "activeMediaBlockStart" event, we must dispatch
    // another event "activeMediablockStop" when the window is resumed from
    // suspend-block.
    bool mShouldSendActiveMediaBlockStopEvent;

   private:
    void AppendAudibleAgentIfNotContained(AudioChannelAgent* aAgent,
                                          AudibleChangedReasons aReason);
    void RemoveAudibleAgentIfContained(AudioChannelAgent* aAgent,
                                       AudibleChangedReasons aReason);

    void AppendAgentAndIncreaseAgentsNum(AudioChannelAgent* aAgent);
    void RemoveAgentAndReduceAgentsNum(AudioChannelAgent* aAgent);

    bool IsFirstAudibleAgent() const;
    bool IsLastAudibleAgent() const;

    void NotifyAudioAudibleChanged(nsPIDOMWindowOuter* aWindow,
                                   AudibleState aAudible,
                                   AudibleChangedReasons aReason);

    void NotifyChannelActive(uint64_t aWindowID, AudioChannel aChannel,
                             bool aActive);
    void MaybeNotifyMediaBlockStart(AudioChannelAgent* aAgent);
  };

  AudioChannelWindow* GetOrCreateWindowData(nsPIDOMWindowOuter* aWindow);

  AudioChannelWindow* GetWindowData(uint64_t aWindowID) const;

  AudioChannelConfig* GetChannelConfig(nsPIDOMWindowOuter* aWindow,
                                       AudioChannel aChannel);

  struct AudioChannelChildStatus final {
    explicit AudioChannelChildStatus(uint64_t aChildID)
        : mChildID(aChildID),
          mActiveTelephonyChannel(false),
          mActiveContentOrNormalChannel(false) {}

    uint64_t mChildID;
    bool mActiveTelephonyChannel;
    bool mActiveContentOrNormalChannel;
  };

  AudioChannelChildStatus* GetChildStatus(uint64_t aChildID) const;

  void RemoveChildStatus(uint64_t aChildID);

  nsTObserverArray<UniquePtr<AudioChannelWindow>> mWindows;

  nsTObserverArray<UniquePtr<AudioChannelChildStatus>> mPlayingChildren;

#ifdef MOZ_WIDGET_GONK
  nsTArray<SpeakerManagerService*> mSpeakerManagerServices;
#endif

  // Raw pointers because BrowserParents must unregister themselves.
  nsTArray<BrowserParent*> mBrowserParents;

  nsCOMPtr<nsIRunnable> mRunnable;

  uint64_t mDefChannelChildID;

  // These boolean are used to know if we have to send an status update to the
  // service running in the main process.
  bool mTelephonyChannel;
  bool mContentOrNormalChannel;
  bool mAnyChannel;

  // This is needed for IPC comunication between
  // AudioChannelServiceChild and this class.
  friend class ContentParent;
  friend class ContentChild;
};

const char* SuspendTypeToStr(const nsSuspendedTypes& aSuspend);
const char* AudibleStateToStr(
    const AudioChannelService::AudibleState& aAudible);
const char* AudibleChangedReasonToStr(
    const AudioChannelService::AudibleChangedReasons& aReason);

}  // namespace mozilla::dom

#endif
