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

#include "mozilla/HalTypes.h"
#include "mozilla/Observer.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/WakeLock.h"
#include "nsTHashMap.h"
#include "nsIAudioManager.h"
#include "nsIObserver.h"

namespace mozilla {
namespace hal {
class SwitchEvent;
typedef Observer<SwitchEvent> SwitchObserver;
}  // namespace hal

namespace dom {
namespace gonk {

class AudioPortCallbackHolder;
class AudioSettingsObserver;
class VolumeCurves;

class AudioManager final : public nsIAudioManager, public nsIObserver {
 public:
  static already_AddRefed<AudioManager> GetInstance();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIAUDIOMANAGER
  NS_DECL_NSIOBSERVER

  // Validate whether the volume index is within the range
  nsresult ValidateVolumeIndex(int32_t aStream, uint32_t aIndex) const;

  // Called when android AudioFlinger in mediaserver is died
  void HandleAudioFlingerDied();

  void HandleHeadphoneSwitchEvent(const hal::SwitchEvent& aEvent);

  class VolumeStreamState {
   public:
    explicit VolumeStreamState(AudioManager& aManager, int32_t aStreamType);
    bool IsDevicesChanged();
    // Returns true if this stream stores separate volume index for each output
    // device. For example, speaker volume of media stream is different from
    // headset volume of media stream. Returns false if this stream shares one
    // volume setting among all output devices, e.g., notification and alarm
    // streams.
    bool IsDeviceSpecificVolume() { return mIsDeviceSpecificVolume; }
    void ClearDevicesChanged();
    void ClearDevicesWithVolumeChange();
    uint32_t GetDevicesWithVolumeChange();
    void InitStreamVolume();
    uint32_t GetMaxIndex();
    uint32_t GetVolumeIndex();
    uint32_t GetVolumeIndex(uint32_t aDevice);
    // Set volume index to all active devices.
    // Active devices are chosen by android AudioPolicyManager.
    nsresult SetVolumeIndexToActiveDevices(uint32_t aIndex);
    // Set volume index to all alias streams for device. Alias streams have same
    // volume.
    nsresult SetVolumeIndexToAliasStreams(uint32_t aIndex, uint32_t aDevice);
    nsresult SetVolumeIndexToConsistentDeviceIfNeeded(uint32_t aIndex,
                                                      uint32_t aDevice);
    nsresult SetVolumeIndex(uint32_t aIndex, uint32_t aDevice,
                            bool aUpdateCache = true);
    // Restore volume index to all devices. Called when AudioFlinger is
    // restarted.
    void RestoreVolumeIndexToAllDevices();

   private:
    AudioManager& mManager;
    const int32_t mStreamType;
    uint32_t mLastDevices = 0;
    uint32_t mDevicesWithVolumeChange = 0;
    bool mIsDevicesChanged = true;
    bool mIsDeviceSpecificVolume = true;
    nsTHashMap<nsUint32HashKey, uint32_t> mVolumeIndexes;
  };

 protected:
  int32_t mPhoneState = PHONE_STATE_CURRENT;

  bool mIsVolumeInited = false;

  // Connected devices that are controlled by setDeviceConnectionState()
  nsTHashMap<nsUint32HashKey, nsCString> mConnectedDevices;

  bool mSwitchDone = true;

  bool mBluetoothA2dpEnabled = false;
#ifdef MOZ_B2G_BT
  bool mA2dpSwitchDone = true;
#endif
  nsTArray<UniquePtr<VolumeStreamState> > mStreamStates;

  RefPtr<mozilla::dom::WakeLock> mWakeLock;

  bool IsFmOutConnected();

  nsresult SetStreamVolumeForDevice(int32_t aStream, uint32_t aIndex,
                                    uint32_t aDevice);
  nsresult SetStreamVolumeIndex(int32_t aStream, uint32_t aIndex);
  nsresult GetStreamVolumeIndex(int32_t aStream, uint32_t* aIndex);

  uint32_t GetDevicesForStream(int32_t aStream);
  uint32_t GetDeviceForStream(int32_t aStream);
  uint32_t GetDeviceForFm();
  // Choose one device as representative of active devices.
  static uint32_t SelectDeviceFromDevices(uint32_t aOutDevices);

 private:
  UniquePtr<mozilla::hal::SwitchObserver> mObserver;
  RefPtr<AudioPortCallbackHolder> mAudioPortCallbackHolder;
#ifdef PRODUCT_MANUFACTURER_QUALCOMM
  UniquePtr<VolumeCurves> mFmVolumeCurves;
#endif
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
                              int32_t* aStream, uint32_t* aDevice,
                              uint32_t* aVolIndex);
  nsTArray<nsString> AudioSettingNames(bool aInitializing);

  void UpdateHeadsetConnectionState(hal::SwitchState aState);
  void UpdateDeviceConnectionState(bool aIsConnected, uint32_t aDevice,
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
