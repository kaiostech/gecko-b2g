/*
 * Copyright 2015 The Android Open Source Project
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

#pragma once

#if ANDROID_VERSION < 33
#  error "Only ANDROID_VERSION >= 33 devices are supported"
#endif

#include <hardware/hwcomposer.h>
#include <hardware/hwcomposer2.h>

#include "android_13/HWC2.h"

// Extend android_13/Hal.h by bringing composer3 namespace and its Composition
// type into hal namespace.
namespace android::hardware::graphics::composer::hal {
namespace V3_0 = ::aidl::android::hardware::graphics::composer3;
using Composition = V3_0::Composition;
}  // namespace android::hardware::graphics::composer::hal

namespace android::HWC2 {

// Extend android_13/HWC2.h by porting class Device from android_10/HWC2.h.
class Device {
  using AidlCapability =
      aidl::android::hardware::graphics::composer3::Capability;

 public:
  explicit Device(std::unique_ptr<android::Hwc2::Composer> composer);

  Device(const std::string& serviceName);

  void registerCallback(ComposerCallback* callback, int32_t sequenceId);

  // Required by HWC2

  std::string dump() const;

  const std::unordered_set<AidlCapability>& getCapabilities() const {
    return mCapabilities;
  };

  uint32_t getMaxVirtualDisplayCount() const;
  hal::Error getDisplayIdentificationData(hal::HWDisplayId hwcDisplayId,
                                          uint8_t* outPort,
                                          std::vector<uint8_t>* outData) const;

  hal::Error createVirtualDisplay(uint32_t width, uint32_t height,
                                  android::ui::PixelFormat* format,
                                  Display** outDisplay);
  void destroyDisplay(hal::HWDisplayId displayId);

  void onHotplug(hal::HWDisplayId displayId, hal::Connection connection);

  // Other Device methods

  Display* getDisplayById(hal::HWDisplayId id);

  android::Hwc2::Composer* getComposer() { return mComposer.get(); }

  hal::HWDisplayId getDefaultDisplayId() { return mDefaultDisplayId; }

  // We buffer most state changes and flush them implicitly with
  // Display::validate, Display::present, and Display::presentOrValidate.
  // This method provides an explicit way to flush state changes to HWC.
  hal::Error flushCommands();

 private:
  // Initialization methods

  void loadCapabilities();

  // Member variables
  std::unique_ptr<android::Hwc2::Composer> mComposer;
  std::unordered_set<AidlCapability> mCapabilities;
  std::unordered_map<hal::HWDisplayId, std::unique_ptr<Display>> mDisplays;
  bool mRegisteredCallback = false;
  hal::HWDisplayId mDefaultDisplayId = HWC_DISPLAY_PRIMARY;
};

}  // namespace android::HWC2

namespace HWC2 {

namespace hal = android::HWC2::hal;

using android::HWC2::ComposerCallback;
using android::HWC2::Device;
using android::HWC2::Display;
using android::HWC2::Layer;

// A hack to create alias of a given enum class. It behaves just like an actual
// enum class. By supporting implicit conversion, it can also be used
// interchangeably with the original enum class.
template <class Base, class EnumType>
class EnumAlias : public Base {
 public:
  EnumAlias(EnumType value) : mValue(value) {}
  operator EnumType() const { return mValue; }
  operator int32_t() const { return static_cast<int32_t>(mValue); }

 private:
  EnumType mValue = static_cast<EnumType>(0);
};

class BlendModeBase {
 public:
  static constexpr auto Invalid = hal::BlendMode::INVALID;
  static constexpr auto None = hal::BlendMode::NONE;
  static constexpr auto Premultiplied = hal::BlendMode::PREMULTIPLIED;
  static constexpr auto Coverage = hal::BlendMode::COVERAGE;
};

class CompositionBase {
 public:
  static constexpr auto Invalid = hal::Composition::INVALID;
  static constexpr auto Client = hal::Composition::CLIENT;
  static constexpr auto Device = hal::Composition::DEVICE;
  static constexpr auto SolidColor = hal::Composition::SOLID_COLOR;
  static constexpr auto Cursor = hal::Composition::CURSOR;
  static constexpr auto Sideband = hal::Composition::SIDEBAND;
};

class ConnectionBase {
 public:
  static constexpr auto Invalid = hal::Connection::INVALID;
  static constexpr auto Connected = hal::Connection::CONNECTED;
  static constexpr auto Disconnected = hal::Connection::DISCONNECTED;
};

class ErrorBase {
 public:
  static constexpr auto None = hal::Error::NONE;
  static constexpr auto BadConfig = hal::Error::BAD_CONFIG;
  static constexpr auto BadDisplay = hal::Error::BAD_DISPLAY;
  static constexpr auto BadLayer = hal::Error::BAD_LAYER;
  static constexpr auto BadParameter = hal::Error::BAD_PARAMETER;
  static constexpr auto HasChanges = static_cast<hal::Error>(ERROR_HAS_CHANGES);
  static constexpr auto NoResources = hal::Error::NO_RESOURCES;
  static constexpr auto NotValidated = hal::Error::NOT_VALIDATED;
  static constexpr auto Unsupported = hal::Error::UNSUPPORTED;
};

class PowerModeBase {
 public:
  static constexpr auto Off = hal::PowerMode::OFF;
  static constexpr auto Doze = hal::PowerMode::DOZE;
  static constexpr auto DozeSuspend = hal::PowerMode::DOZE_SUSPEND;
  static constexpr auto On = hal::PowerMode::ON;
};

class VsyncBase {
 public:
  static constexpr auto Invalid = hal::Vsync::INVALID;
  static constexpr auto Enable = hal::Vsync::ENABLE;
  static constexpr auto Disable = hal::Vsync::DISABLE;
};

using BlendMode = EnumAlias<BlendModeBase, hal::BlendMode>;
using Composition = EnumAlias<CompositionBase, hal::Composition>;
using Connection = EnumAlias<ConnectionBase, hal::Connection>;
using Error = EnumAlias<ErrorBase, hal::Error>;
using PowerMode = EnumAlias<PowerModeBase, hal::PowerMode>;
using Vsync = EnumAlias<VsyncBase, hal::Vsync>;

}  // namespace HWC2
