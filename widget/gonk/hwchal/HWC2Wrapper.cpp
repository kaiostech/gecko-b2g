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

#if ANDROID_VERSION < 33
#  error "Only ANDROID_VERSION >= 33 devices are supported"
#endif

#include "HWC2Wrapper.h"

namespace android {
namespace HWC2 {

using android::HWC2::ComposerCallback;
using android::HWC2::Display;
using namespace android::hardware::graphics::composer::hal;

Device::Device(std::unique_ptr<android::Hwc2::Composer> composer)
    : mComposer(std::move(composer)) {
  loadCapabilities();
}

Device::Device(const std::string& serviceName)
    : mComposer(android::Hwc2::Composer::create(serviceName)) {
  loadCapabilities();
}

void Device::registerCallback(ComposerCallback* callback, int32_t sequenceId) {
  mComposer->registerCallback(*callback);
}

std::string Device::dump() const { return mComposer->dumpDebugInfo(); }

uint32_t Device::getMaxVirtualDisplayCount() const {
  return mComposer->getMaxVirtualDisplayCount();
}

Error Device::getDisplayIdentificationData(
    HWDisplayId hwcDisplayId, uint8_t* outPort,
    std::vector<uint8_t>* outData) const {
  auto intError =
      mComposer->getDisplayIdentificationData(hwcDisplayId, outPort, outData);
  return static_cast<Error>(intError);
}

Error Device::createVirtualDisplay(uint32_t width, uint32_t height,
                                   android::ui::PixelFormat* format,
                                   Display** outDisplay) {
  ALOGI("Creating virtual display");

  HWDisplayId displayId = 0;
  auto intError =
      mComposer->createVirtualDisplay(width, height, format, &displayId);
  auto error = static_cast<Error>(intError);
  if (error != Error::NONE) {
    return error;
  }

  auto display = std::make_unique<impl::Display>(
      *mComposer.get(), mCapabilities, displayId, DisplayType::VIRTUAL);
  display->setConnected(true);
  *outDisplay = display.get();
  mDisplays.emplace(displayId, std::move(display));
  ALOGI("Created virtual display");
  return Error::NONE;
}

void Device::destroyDisplay(HWDisplayId displayId) {
  ALOGI("Destroying display %" PRIu64, displayId);
  mDisplays.erase(displayId);
}

void Device::onHotplug(HWDisplayId displayId, Connection connection) {
  if (connection == Connection::CONNECTED) {
    // If we get a hotplug connected event for a display we already have,
    // destroy the display and recreate it. This will force us to requery
    // the display params and recreate all layers on that display.
    auto oldDisplay = getDisplayById(displayId);
    if (oldDisplay != nullptr && oldDisplay->isConnected()) {
      ALOGI(
          "Hotplug connecting an already connected display."
          " Clearing old display state.");
    }
    mDisplays.erase(displayId);

    mDefaultDisplayId = displayId;

    // FIXME: we can't get display type from AOSP 13 ComposerHal.
#if 0
    DisplayType displayType;
    auto intError = mComposer->getDisplayType(
        displayId,
        reinterpret_cast<Hwc2::IComposerClient::DisplayType*>(&displayType));
    auto error = static_cast<Error>(intError);
    if (error != Error::NONE) {
      ALOGE("getDisplayType(%" PRIu64
            ") failed: %s (%d). "
            "Aborting hotplug attempt.",
            displayId, to_string(error).c_str(), intError);
      return;
    }
#else
    auto displayType = DisplayType::PHYSICAL;
#endif

    auto newDisplay = std::make_unique<impl::Display>(
        *mComposer.get(), mCapabilities, displayId, displayType);
    newDisplay->setConnected(true);
    mDisplays.emplace(displayId, std::move(newDisplay));
    mComposer->onHotplugConnect(displayId);
  } else if (connection == Connection::DISCONNECTED) {
    // The display will later be destroyed by a call to
    // destroyDisplay(). For now we just mark it disconnected.
    auto display = getDisplayById(displayId);
    if (display) {
      display->setConnected(false);
      mComposer->onHotplugDisconnect(displayId);
    } else {
      ALOGW("Attempted to disconnect unknown display %" PRIu64, displayId);
    }
  }
}

// Other Device methods

Display* Device::getDisplayById(HWDisplayId id) {
  auto iter = mDisplays.find(id);
  return iter == mDisplays.end() ? nullptr : iter->second.get();
}

// Device initialization methods

void Device::loadCapabilities() {
  static_assert(sizeof(Capability) == sizeof(int32_t),
                "Capability size has changed");
  auto capabilities = mComposer->getCapabilities();
  for (auto capability : capabilities) {
    mCapabilities.emplace(capability);
  }
}

#if ANDROID_VERSION >= 33
Error Device::flushCommands(Display display) {
  // FIXME: seems confused with display/displayId from AOSP 13/14 ComposerHal
  return static_cast<Error>(mComposer->executeCommands(display.getId()));
}
#else
Error Device::flushCommands() {
  return static_cast<Error>(mComposer->executeCommands());
}
#endif
}  // namespace HWC2
}  // namespace android
