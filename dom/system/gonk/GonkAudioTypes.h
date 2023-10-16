/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GONK_AUDIO_TYPES_H
#define GONK_AUDIO_TYPES_H

#include <set>

#include <system/audio.h>
#if ANDROID_VERSION >= 30
#  include <media/AudioContainers.h>
#endif

#if ANDROID_VERSION < 30
// From audio-base-utils.h
static inline constexpr audio_devices_t AUDIO_DEVICE_OUT_ALL_A2DP_ARRAY[] = {
    AUDIO_DEVICE_OUT_BLUETOOTH_A2DP,            // 0x00000080u,
    AUDIO_DEVICE_OUT_BLUETOOTH_A2DP_HEADPHONES, // 0x00000100u,
    AUDIO_DEVICE_OUT_BLUETOOTH_A2DP_SPEAKER,    // 0x00000200u,
};

namespace android {
// From AudioContainers.h.
using DeviceTypeSet = std::set<audio_devices_t>;
}  // namespace android
#endif

#endif
