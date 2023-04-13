/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_AUDIOSYSTEM_H_
#define ANDROID_AUDIOSYSTEM_H_

#pragma GCC visibility push(default)

#include <utils/RefBase.h>
#include <utils/threads.h>
#include "audio.h"
#include "audio_policy.h"
#include "IAudioFlinger.h"
#include "IAudioPolicyServiceClient.h"

namespace android {

typedef void (*audio_error_callback)(status_t err);

class IAudioPolicyService;
class String8;

class AudioSystem {
 public:
  // FIXME Declare in binder opcode order, similarly to IAudioFlinger.h and
  // IAudioFlinger.cpp

  /* These are static methods to control the system-wide AudioFlinger
   * only privileged processes can have access to them
   */

  // mute/unmute microphone
  static status_t muteMicrophone(bool state);
  static status_t isMicrophoneMuted(bool* state);

  // set/get master volume
  static status_t setMasterVolume(float value);
  static status_t getMasterVolume(float* volume);

  // mute/unmute audio outputs
  static status_t setMasterMute(bool mute);
  static status_t getMasterMute(bool* mute);

  // set/get stream volume on specified output
  static status_t setStreamVolume(audio_stream_type_t stream, float value,
                                  audio_io_handle_t output);
  static status_t getStreamVolume(audio_stream_type_t stream, float* volume,
                                  audio_io_handle_t output);

  // mute/unmute stream
  static status_t setStreamMute(audio_stream_type_t stream, bool mute);
  static status_t getStreamMute(audio_stream_type_t stream, bool* mute);

  // set audio mode in audio hardware
  static status_t setMode(audio_mode_t mode);

  // returns true in *state if tracks are active on the specified stream or have
  // been active in the past inPastMs milliseconds
  static status_t isStreamActive(audio_stream_type_t stream, bool* state,
                                 uint32_t inPastMs);

  // set/get audio hardware parameters. The function accepts a list of
  // parameters key value pairs in the form: key1=value1;key2=value2;... Some
  // keys are reserved for standard parameters (See AudioParameter class). The
  // versions with audio_io_handle_t are intended for internal media framework
  // use only.
  static status_t setParameters(audio_io_handle_t ioHandle,
                                const String8& keyValuePairs);
  static String8 getParameters(audio_io_handle_t ioHandle, const String8& keys);

#if ANDROID_VERSION >= 30
  static void addErrorCallback(audio_error_callback cb);
#else
  static void setErrorCallback(audio_error_callback cb);
#endif

  // helper function to obtain AudioFlinger service handle
  static const sp<IAudioFlinger> get_audio_flinger();

  static float linearToLog(int volume);
  static int logToLinear(float volume);

  // Returned samplingRate and frameCount output values are guaranteed
  // to be non-zero if status == NO_ERROR
  // FIXME This API assumes a route, and so should be deprecated.
  static status_t getOutputSamplingRate(uint32_t* samplingRate,
                                        audio_stream_type_t stream);
  // FIXME This API assumes a route, and so should be deprecated.
  static status_t getOutputFrameCount(size_t* frameCount,
                                      audio_stream_type_t stream);
  // FIXME This API assumes a route, and so should be deprecated.
  static status_t getOutputLatency(uint32_t* latency,
                                   audio_stream_type_t stream);

  // return status NO_ERROR implies *buffSize > 0
  // FIXME This API assumes a route, and so should deprecated.
  static status_t getInputBufferSize(uint32_t sampleRate, audio_format_t format,
                                     audio_channel_mask_t channelMask,
                                     size_t* buffSize);

  static status_t setVoiceVolume(float volume);

  // return the number of audio frames written by AudioFlinger to audio HAL and
  // audio dsp to DAC since the specified output has exited standby.
  // returned status (from utils/Errors.h) can be:
  // - NO_ERROR: successful operation, halFrames and dspFrames point to valid
  // data
  // - INVALID_OPERATION: Not supported on current hardware platform
  // - BAD_VALUE: invalid parameter
  // NOTE: this feature is not supported on all hardware platforms and it is
  // necessary to check returned status before using the returned values.
  static status_t getRenderPosition(audio_io_handle_t output,
                                    uint32_t* halFrames, uint32_t* dspFrames);

  // return the number of input frames lost by HAL implementation, or 0 if the
  // handle is invalid
  static uint32_t getInputFramesLost(audio_io_handle_t ioHandle);

  //
  // IAudioPolicyService interface (see AudioPolicyInterface for method
  // descriptions)
  //
  static status_t setDeviceConnectionState(audio_devices_t device,
                                           audio_policy_dev_state_t state,
                                           const char* device_address,
                                           const char* device_name,
                                           audio_format_t encodedFormat);
  static audio_policy_dev_state_t getDeviceConnectionState(
      audio_devices_t device, const char* device_address);
#if ANDROID_VERSION >= 30
  static status_t setPhoneState(audio_mode_t state, uid_t uid);
#else
  static status_t setPhoneState(audio_mode_t state);
#endif
  static status_t setForceUse(audio_policy_force_use_t usage,
                              audio_policy_forced_cfg_t config);
  static audio_policy_forced_cfg_t getForceUse(audio_policy_force_use_t usage);

  static status_t initStreamVolume(audio_stream_type_t stream, int indexMin,
                                   int indexMax);
  static status_t setStreamVolumeIndex(audio_stream_type_t stream, int index,
                                       audio_devices_t device);
  static status_t getStreamVolumeIndex(audio_stream_type_t stream, int* index,
                                       audio_devices_t device);
  static audio_devices_t getDevicesForStream(audio_stream_type_t stream);
  static audio_io_handle_t getOutputForEffect(const effect_descriptor_t* desc);
  static status_t registerEffect(const effect_descriptor_t* desc,
                                 audio_io_handle_t io, uint32_t strategy,
                                 audio_session_t session, int id);
  static status_t unregisterEffect(int id);

  static const sp<IAudioPolicyService> get_audio_policy_service();

  static status_t setMasterMono(bool mono);
  static status_t getMasterMono(bool *mono);

  static status_t setMasterBalance(float balance);
  static status_t getMasterBalance(float *balance);

  static float getStreamVolumeDB(audio_stream_type_t stream, int index,
                                 audio_devices_t device);

  static status_t setAssistantUid(uid_t uid);

  class AudioPortCallback : public RefBase {
   public:
    AudioPortCallback() {}
    virtual ~AudioPortCallback() {}

    virtual void onAudioPortListUpdate() = 0;
    virtual void onAudioPatchListUpdate() = 0;
    virtual void onServiceDied() = 0;
  };

  static status_t addAudioPortCallback(const sp<AudioPortCallback>& callback);
  static status_t removeAudioPortCallback(
      const sp<AudioPortCallback>& callback);

 private:
  class AudioFlingerClient : public IBinder::DeathRecipient,
                             public BnAudioFlingerClient {
   public:
    AudioFlingerClient()
        : mInBuffSize(0),
          mInSamplingRate(0),
          mInFormat(AUDIO_FORMAT_DEFAULT),
          mInChannelMask(AUDIO_CHANNEL_NONE) {}

    // DeathRecipient
    virtual void binderDied(const wp<IBinder>& who);

    // IAudioFlingerClient

    // indicate a change in the configuration of an output or input: keeps the
    // cached values for output/input parameters up-to-date in client process
    virtual void ioConfigChanged(audio_io_config_event event,
                                 const sp<AudioIoDescriptor>& ioDesc);

   private:
    // cached values for recording getInputBufferSize() queries
    size_t mInBuffSize;  // zero indicates cache is invalid
    uint32_t mInSamplingRate;
    audio_format_t mInFormat;
    audio_channel_mask_t mInChannelMask;
  };

  class AudioPolicyServiceClient : public IBinder::DeathRecipient,
                                   public BnAudioPolicyServiceClient {
   public:
    AudioPolicyServiceClient() {}

    // DeathRecipient
    virtual void binderDied(const wp<IBinder>& who);
  };

  static sp<AudioFlingerClient> gAudioFlingerClient;
  static sp<AudioPolicyServiceClient> gAudioPolicyServiceClient;
  friend class AudioFlingerClient;
  friend class AudioPolicyServiceClient;

  static Mutex gLock;  // protects gAudioFlinger and gAudioErrorCallback,
  static sp<IAudioFlinger> gAudioFlinger;
  static audio_error_callback gAudioErrorCallback;

  static size_t gInBuffSize;
  // previous parameters for recording buffer size queries
  static uint32_t gPrevInSamplingRate;
  static audio_format_t gPrevInFormat;

  static sp<IAudioPolicyService> gAudioPolicyService;
};

};  // namespace android

#pragma GCC visibility pop

#endif /*ANDROID_AUDIOSYSTEM_H_*/
