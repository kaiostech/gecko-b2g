/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GONK_AUDIO_SYSTEM_H
#define GONK_AUDIO_SYSTEM_H

#include "GonkAudioTypes.h"

#include <media/AudioSystem.h>

namespace android {

class GonkAudioSystem : private AudioSystem {
 public:
  // clang-format off
  using AudioSystem::AudioPortCallback;
  using AudioSystem::isMicrophoneMuted;
  using AudioSystem::getForceUse;
  using AudioSystem::getParameters;
  using AudioSystem::initStreamVolume;
  using AudioSystem::setStreamVolumeIndex;
  using AudioSystem::getStreamVolumeDB;
  using AudioSystem::addAudioPortCallback;
  using AudioSystem::removeAudioPortCallback;
  // clang-format on

  static status_t setMasterMono(bool mono);

  static status_t setMasterBalance(float balance);

  static status_t setMasterVolume(float value);

  static status_t muteMicrophone(bool state);

  static status_t setForceUse(audio_policy_force_use_t usage,
                              audio_policy_forced_cfg_t config);

  static status_t setParameters(const String8& keyValuePairs);

  static void setErrorCallback(audio_error_callback cb);

  static status_t setPhoneState(audio_mode_t state,
                                uid_t uid = AUDIO_UID_INVALID);

  static status_t setAssistantUid(uid_t uid);

  static status_t setDeviceConnectionState(
      audio_devices_t device, audio_policy_dev_state_t state,
      const char* deviceAddress, const char* deviceName = "",
      audio_format_t encodedFormat = AUDIO_FORMAT_DEFAULT);

  static status_t setDeviceConnected(
      audio_devices_t device, bool connected, const char* deviceAddress,
      const char* deviceName = "",
      audio_format_t encodedFormat = AUDIO_FORMAT_DEFAULT);

  static DeviceTypeSet getDeviceTypesForStream(audio_stream_type_t stream);

  // Deprecated.
  static audio_devices_t getDevicesForStream(audio_stream_type_t stream);

 private:
  static DeviceTypeSet deviceBitMaskToTypeSet(audio_devices_t devices);
};

}  // namespace android

#endif  // GONK_AUDIO_SYSTEM_H
