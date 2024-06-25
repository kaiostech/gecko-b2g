/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GonkAudioSystem.h"

#undef LOG_TAG
#define LOG_TAG "GonkAudioSystem"
// #define LOG_NDEBUG 0

#include <utils/Log.h>
#include <utils/Mutex.h>

#if ANDROID_VERSION >= 33
#  include <media/AidlConversion.h>
#endif

namespace android {

status_t GonkAudioSystem::setMasterMono(bool mono) {
  ALOGD("%s, %d", __func__, mono);
  return AudioSystem::setMasterMono(mono);
}

status_t GonkAudioSystem::setMasterBalance(float balance) {
  ALOGD("%s, %f", __func__, balance);
  return AudioSystem::setMasterBalance(balance);
}

status_t GonkAudioSystem::setMasterVolume(float value) {
  ALOGD("%s, %f", __func__, value);
  return AudioSystem::setMasterVolume(value);
}

status_t GonkAudioSystem::muteMicrophone(bool state) {
  ALOGD("%s, %d", __func__, state);
  return AudioSystem::muteMicrophone(state);
}

status_t GonkAudioSystem::setForceUse(audio_policy_force_use_t usage,
                                      audio_policy_forced_cfg_t config) {
  ALOGD("%s, usage %d, config %d", __func__, int(usage), int(config));
  return AudioSystem::setForceUse(usage, config);
}

status_t GonkAudioSystem::setParameters(const String8& keyValuePairs) {
  ALOGD("%s, %s", __func__, keyValuePairs.c_str());
  return AudioSystem::setParameters(keyValuePairs);
}

void GonkAudioSystem::setErrorCallback(audio_error_callback cb) {
#if ANDROID_VERSION >= 30
  static Mutex sLock;
  static audio_error_callback sCallback = nullptr;

  Mutex::Autolock _l(sLock);
  if (sCallback == cb) {
    return;
  }
  if (sCallback) {
    AudioSystem::removeErrorCallback(reinterpret_cast<uintptr_t>(sCallback));
  }
  if (cb) {
    AudioSystem::addErrorCallback(cb);
  }
  sCallback = cb;
#else
  AudioSystem::setErrorCallback(cb);
#endif
}

status_t GonkAudioSystem::setAssistantUid(uid_t uid) {
  ALOGD("%s, %d", __func__, uid);
#if ANDROID_VERSION >= 33
  return AudioSystem::setAssistantServicesUids({uid});
#else
  return AudioSystem::setAssistantUid(uid);
#endif
}

status_t GonkAudioSystem::setPhoneState(audio_mode_t state, uid_t uid) {
  ALOGD("%s, state %d, uid %d", __func__, int(state), uid);
#if ANDROID_VERSION >= 30
  return AudioSystem::setPhoneState(state, uid);
#else
  return AudioSystem::setPhoneState(state);
#endif
}

status_t GonkAudioSystem::setDeviceConnectionState(
    audio_devices_t device, audio_policy_dev_state_t state,
    const char* deviceAddress, const char* deviceName,
    audio_format_t encodedFormat) {
  ALOGD("%s, device 0x%x, state %d, addr %s, name %s, format %d", __func__,
        int(device), int(state), deviceAddress, deviceName, int(encodedFormat));
#if ANDROID_VERSION >= 33
  ::android::media::audio::common::AudioPortDeviceExt deviceExt;
  deviceExt.device = VALUE_OR_RETURN_STATUS(
      legacy2aidl_audio_device_AudioDevice(device, deviceAddress));

  ::android::media::audio::common::AudioPort port;
  port.name = deviceName;
  port.ext = deviceExt;
  return AudioSystem::setDeviceConnectionState(state, port, encodedFormat);
#else
  return AudioSystem::setDeviceConnectionState(device, state, deviceAddress,
                                               deviceName, encodedFormat);
#endif
}

status_t GonkAudioSystem::setDeviceConnected(audio_devices_t device,
                                             bool connected,
                                             const char* deviceAddress,
                                             const char* deviceName,
                                             audio_format_t encodedFormat) {
  auto state = connected ? AUDIO_POLICY_DEVICE_STATE_AVAILABLE
                         : AUDIO_POLICY_DEVICE_STATE_UNAVAILABLE;
  return setDeviceConnectionState(device, state, deviceAddress, deviceName,
                                  encodedFormat);
}

DeviceTypeSet GonkAudioSystem::deviceBitMaskToTypeSet(audio_devices_t devices) {
  DeviceTypeSet typeSet;
  uint32_t dir = devices & AUDIO_DEVICE_BIT_IN;
  uint32_t types = devices & ~AUDIO_DEVICE_BIT_IN;
  while (types) {
    uint32_t type = types & -types;  // get lowest bit
    types ^= type;                   // clear lowest bit
    typeSet.insert(static_cast<audio_devices_t>(dir | type));
  }
  return typeSet;
}

DeviceTypeSet GonkAudioSystem::getDeviceTypesForStream(
    audio_stream_type_t stream) {
#if ANDROID_VERSION >= 33
  auto attr = AudioSystem::streamTypeToAttributes(stream);
  AudioDeviceTypeAddrVector devices;
  status_t err = AudioSystem::getDevicesForAttributes(attr, &devices, true);
  return err == OK ? getAudioDeviceTypes(devices) : DeviceTypeSet();
#else
  return deviceBitMaskToTypeSet(AudioSystem::getDevicesForStream(stream));
#endif
}

audio_devices_t GonkAudioSystem::getDevicesForStream(
    audio_stream_type_t stream) {
#if ANDROID_VERSION >= 33
  return deviceTypesToBitMask(getDeviceTypesForStream(stream));
#else
  return AudioSystem::getDevicesForStream(stream);
#endif
}

}  // namespace android
