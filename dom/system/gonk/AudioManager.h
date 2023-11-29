/* Copyright 2012 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef mozilla_dom_system_b2g_audiomanager_h__
#define mozilla_dom_system_b2g_audiomanager_h__

#include "GonkAudioTypes.h"
#include "mozilla/HalTypes.h"
#include "mozilla/Observer.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/WakeLock.h"
#include "nsIAudioManager.h"
#include "nsIObserver.h"

#include <unordered_map>

namespace mozilla {
namespace hal {
class SwitchEvent;
typedef Observer<SwitchEvent> SwitchObserver;
}  // namespace hal

namespace dom {
namespace gonk {

class AudioPortCallbackHolder;
class AudioSettingsObserver;

class AudioManager final : public nsIAudioManager, public nsIObserver {
  using DeviceTypeSet = android::DeviceTypeSet;

 public:
  static already_AddRefed<AudioManager> GetInstance();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIAUDIOMANAGER
  NS_DECL_NSIOBSERVER

  // Validate whether the volume index is within the range
  nsresult ValidateVolumeIndex(audio_stream_type_t aStream,
                               uint32_t aIndex) const;

  // Called when android AudioFlinger in mediaserver is died
  void HandleAudioFlingerDied();

  void HandleHeadphoneSwitchEvent(const hal::SwitchEvent& aEvent);

  class VolumeStreamState {
   public:
    explicit VolumeStreamState(AudioManager& aManager,
                               audio_stream_type_t aStreamType);
    bool IsDevicesChanged();
    // Returns true if this stream stores separate volume index for each output
    // device. For example, speaker volume of media stream is different from
    // headset volume of media stream. Returns false if this stream shares one
    // volume setting among all output devices, e.g., notification and alarm
    // streams.
    bool IsDeviceSpecificVolume() { return mIsDeviceSpecificVolume; }
    void ClearDevicesChanged();
    void ClearDevicesWithVolumeChange();
    DeviceTypeSet GetDevicesWithVolumeChange();
    void InitStreamVolume();
    uint32_t GetMaxIndex();
    uint32_t GetMinIndex();
    uint32_t GetVolumeIndex();
    uint32_t GetVolumeIndex(audio_devices_t aDevice);
    // Set volume index to all active devices.
    // Active devices are chosen by android AudioPolicyManager.
    nsresult SetVolumeIndexToActiveDevices(uint32_t aIndex);
    // Set volume index to all alias streams for device. Alias streams have same
    // volume.
    nsresult SetVolumeIndexToAliasStreams(uint32_t aIndex,
                                          audio_devices_t aDevice);
    nsresult SetVolumeIndexToConsistentDeviceIfNeeded(uint32_t aIndex,
                                                      audio_devices_t aDevice);
    nsresult SetVolumeIndex(uint32_t aIndex, audio_devices_t aDevice,
                            bool aUpdateCache = true);
    // Restore volume index to all devices. Called when AudioFlinger is
    // restarted.
    void RestoreVolumeIndexToAllDevices();

   private:
    AudioManager& mManager;
    const audio_stream_type_t mStreamType;
    DeviceTypeSet mLastDevices;
    DeviceTypeSet mDevicesWithVolumeChange;
    bool mIsDevicesChanged = true;
    bool mIsDeviceSpecificVolume = true;
    std::unordered_map<audio_devices_t, uint32_t> mVolumeIndexes;
  };

 protected:
  audio_mode_t mPhoneState = AUDIO_MODE_CURRENT;

  bool mIsVolumeInited = false;

  // Connected devices that are controlled by setDeviceConnectionState()
  std::unordered_map<audio_devices_t, nsCString> mConnectedDevices;

  bool mSwitchDone = true;

  bool mBluetoothA2dpEnabled = false;
#ifdef MOZ_B2G_BT
  bool mA2dpSwitchDone = true;
#endif
  nsTArray<UniquePtr<VolumeStreamState> > mStreamStates;

  RefPtr<mozilla::dom::WakeLock> mWakeLock;

  bool IsFmOutConnected();

  nsresult SetStreamVolumeForDevice(audio_stream_type_t aStream,
                                    uint32_t aIndex, audio_devices_t aDevice);
  nsresult SetStreamVolumeIndex(audio_stream_type_t aStream, uint32_t aIndex);
  nsresult GetStreamVolumeIndex(audio_stream_type_t aStream, uint32_t* aIndex);

  audio_devices_t GetDeviceForStream(audio_stream_type_t aStream);
  audio_devices_t GetDeviceForFm();

 private:
  UniquePtr<mozilla::hal::SwitchObserver> mObserver;
  RefPtr<AudioPortCallbackHolder> mAudioPortCallbackHolder;
#ifdef MOZ_B2G_RIL
  bool mMuteCallToRIL = false;
  // mIsMicMuted is only used for toggling mute call to RIL.
  bool mIsMicMuted = false;
#endif

  float mFmContentVolume = 1.0f;

  bool mMasterMono = false;

  float mMasterBalance = 0.5f;

  void HandleBluetoothStatusChanged(nsISupports* aSubject, const char* aTopic,
                                    const nsCString aAddress);

  // Set FM output device according to the current routing of music stream.
  void SetFmRouting();
  // Sync FM volume from music stream.
  void UpdateFmVolume();
  // Mute/unmute FM audio if supported. This is necessary when setting FM audio
  // path on some platforms. Note that this is an internal API and should not be
  // called directly.
  void SetFmMuted(bool aMuted);

  // We store the audio setting in the database, these are related functions.
  void ReadAudioSettings();
  void ReadAudioSettingsFinished();
  void MaybeWriteVolumeSettings(bool aForce = false);
  void OnAudioSettingChanged(const nsAString& aName, const nsAString& aValue);
  nsresult ParseVolumeSetting(const nsAString& aName, const nsAString& aValue,
                              audio_stream_type_t* aStream,
                              audio_devices_t* aDevice, uint32_t* aVolIndex);
  nsTArray<nsString> AudioSettingNames(bool aInitializing);

  void UpdateHeadsetConnectionState(hal::SwitchState aState);
  void UpdateDeviceConnectionState(bool aIsConnected, audio_devices_t aDevice,
                                   const nsCString& aDeviceAddress = ""_ns);
  void SetAllDeviceConnectionStates();

  void CreateWakeLock();
  void ReleaseWakeLock();

  nsresult SetParameters(const char* aFormat, ...);
  nsAutoCString GetParameters(const char* aKeys);

  AudioManager();
  ~AudioManager();
  void Init();

  RefPtr<AudioSettingsObserver> mAudioSettingsObserver;

  friend class AudioSettingsGetCallback;
  friend class AudioSettingsObserver;
  friend class GonkAudioPortCallback;
  friend class VolumeStreamState;
};

} /* namespace gonk */
} /* namespace dom */
} /* namespace mozilla */

#endif  // mozilla_dom_system_b2g_audiomanager_h__
