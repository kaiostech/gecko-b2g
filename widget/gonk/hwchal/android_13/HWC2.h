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

#include <android-base/expected.h>
#include <android-base/thread_annotations.h>
#include <ftl/future.h>
#include <gui/HdrMetadata.h>
#include <math/mat4.h>
#include <ui/HdrCapabilities.h>
#include <ui/Region.h>
#include <ui/StaticDisplayInfo.h>
#include <utils/Log.h>
#include <utils/StrongPointer.h>
#include <utils/Timers.h>

#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ComposerHal.h"
#include "Hal.h"

#include <aidl/android/hardware/graphics/common/DisplayDecorationSupport.h>
#include <aidl/android/hardware/graphics/composer3/Capability.h>
#include <aidl/android/hardware/graphics/composer3/ClientTargetPropertyWithBrightness.h>
#include <aidl/android/hardware/graphics/composer3/Color.h>
#include <aidl/android/hardware/graphics/composer3/Composition.h>
#include <aidl/android/hardware/graphics/composer3/DisplayCapability.h>

namespace android {

class Fence;
class FloatRect;
class GraphicBuffer;
class TestableSurfaceFlinger;
struct DisplayedFrameStats;

namespace Hwc2 {
class Composer;
} // namespace Hwc2

namespace HWC2 {

class Layer;

namespace hal = android::hardware::graphics::composer::hal;

// Implement this interface to receive hardware composer events.
//
// These callback functions will generally be called on a hwbinder thread, but
// when first registering the callback the onComposerHalHotplug() function will
// immediately be called on the thread calling registerCallback().
struct ComposerCallback {
    virtual void onComposerHalHotplug(hal::HWDisplayId, hal::Connection) = 0;
    virtual void onComposerHalRefresh(hal::HWDisplayId) = 0;
    virtual void onComposerHalVsync(hal::HWDisplayId, int64_t timestamp,
                                    std::optional<hal::VsyncPeriodNanos>) = 0;
    virtual void onComposerHalVsyncPeriodTimingChanged(hal::HWDisplayId,
                                                       const hal::VsyncPeriodChangeTimeline&) = 0;
    virtual void onComposerHalSeamlessPossible(hal::HWDisplayId) = 0;
    virtual void onComposerHalVsyncIdle(hal::HWDisplayId) = 0;

protected:
    ~ComposerCallback() = default;
};

// Convenience C++ class to access per display functions directly.
class Display {
public:
    virtual ~Display();

    virtual hal::HWDisplayId getId() const = 0;
    virtual bool isConnected() const = 0;
    virtual void setConnected(bool connected) = 0; // For use by Device only
    virtual bool hasCapability(
            aidl::android::hardware::graphics::composer3::DisplayCapability) const = 0;
    virtual bool isVsyncPeriodSwitchSupported() const = 0;
    virtual bool hasDisplayIdleTimerCapability() const = 0;
    virtual void onLayerDestroyed(hal::HWLayerId layerId) = 0;

    [[nodiscard]] virtual hal::Error acceptChanges() = 0;
    [[nodiscard]] virtual base::expected<std::shared_ptr<HWC2::Layer>, hal::Error>
    createLayer() = 0;
    [[nodiscard]] virtual hal::Error getChangedCompositionTypes(
            std::unordered_map<Layer*, aidl::android::hardware::graphics::composer3::Composition>*
                    outTypes) = 0;
    [[nodiscard]] virtual hal::Error getColorModes(std::vector<hal::ColorMode>* outModes) const = 0;
    // Returns a bitmask which contains HdrMetadata::Type::*.
    [[nodiscard]] virtual int32_t getSupportedPerFrameMetadata() const = 0;
    [[nodiscard]] virtual hal::Error getRenderIntents(
            hal::ColorMode colorMode, std::vector<hal::RenderIntent>* outRenderIntents) const = 0;
    [[nodiscard]] virtual hal::Error getDataspaceSaturationMatrix(hal::Dataspace dataspace,
                                                                  android::mat4* outMatrix) = 0;

    [[nodiscard]] virtual hal::Error getName(std::string* outName) const = 0;
    [[nodiscard]] virtual hal::Error getRequests(
            hal::DisplayRequest* outDisplayRequests,
            std::unordered_map<Layer*, hal::LayerRequest>* outLayerRequests) = 0;
    [[nodiscard]] virtual hal::Error getConnectionType(ui::DisplayConnectionType*) const = 0;
    [[nodiscard]] virtual hal::Error supportsDoze(bool* outSupport) const = 0;
    [[nodiscard]] virtual hal::Error getHdrCapabilities(
            android::HdrCapabilities* outCapabilities) const = 0;
    [[nodiscard]] virtual hal::Error getDisplayedContentSamplingAttributes(
            hal::PixelFormat* outFormat, hal::Dataspace* outDataspace,
            uint8_t* outComponentMask) const = 0;
    [[nodiscard]] virtual hal::Error setDisplayContentSamplingEnabled(bool enabled,
                                                                      uint8_t componentMask,
                                                                      uint64_t maxFrames) const = 0;
    [[nodiscard]] virtual hal::Error getDisplayedContentSample(
            uint64_t maxFrames, uint64_t timestamp,
            android::DisplayedFrameStats* outStats) const = 0;
    [[nodiscard]] virtual hal::Error getReleaseFences(
            std::unordered_map<Layer*, android::sp<android::Fence>>* outFences) const = 0;
    [[nodiscard]] virtual hal::Error present(android::sp<android::Fence>* outPresentFence) = 0;
    [[nodiscard]] virtual hal::Error setClientTarget(
            uint32_t slot, const android::sp<android::GraphicBuffer>& target,
            const android::sp<android::Fence>& acquireFence, hal::Dataspace dataspace) = 0;
    [[nodiscard]] virtual hal::Error setColorMode(hal::ColorMode mode,
                                                  hal::RenderIntent renderIntent) = 0;
    [[nodiscard]] virtual hal::Error setColorTransform(const android::mat4& matrix) = 0;
    [[nodiscard]] virtual hal::Error setOutputBuffer(
            const android::sp<android::GraphicBuffer>& buffer,
            const android::sp<android::Fence>& releaseFence) = 0;
    [[nodiscard]] virtual hal::Error setPowerMode(hal::PowerMode mode) = 0;
    [[nodiscard]] virtual hal::Error setVsyncEnabled(hal::Vsync enabled) = 0;
    [[nodiscard]] virtual hal::Error validate(nsecs_t expectedPresentTime, uint32_t* outNumTypes,
                                              uint32_t* outNumRequests) = 0;
    [[nodiscard]] virtual hal::Error presentOrValidate(nsecs_t expectedPresentTime,
                                                       uint32_t* outNumTypes,
                                                       uint32_t* outNumRequests,
                                                       android::sp<android::Fence>* outPresentFence,
                                                       uint32_t* state) = 0;
    [[nodiscard]] virtual ftl::Future<hal::Error> setDisplayBrightness(
            float brightness, float brightnessNits,
            const Hwc2::Composer::DisplayBrightnessOptions& options) = 0;
    [[nodiscard]] virtual hal::Error setActiveConfigWithConstraints(
            hal::HWConfigId configId, const hal::VsyncPeriodChangeConstraints& constraints,
            hal::VsyncPeriodChangeTimeline* outTimeline) = 0;
    [[nodiscard]] virtual hal::Error setBootDisplayConfig(hal::HWConfigId configId) = 0;
    [[nodiscard]] virtual hal::Error clearBootDisplayConfig() = 0;
    [[nodiscard]] virtual hal::Error getPreferredBootDisplayConfig(
            hal::HWConfigId* configId) const = 0;
    [[nodiscard]] virtual hal::Error setAutoLowLatencyMode(bool on) = 0;
    [[nodiscard]] virtual hal::Error getSupportedContentTypes(
            std::vector<hal::ContentType>*) const = 0;
    [[nodiscard]] virtual hal::Error setContentType(hal::ContentType) = 0;
    [[nodiscard]] virtual hal::Error getClientTargetProperty(
            aidl::android::hardware::graphics::composer3::ClientTargetPropertyWithBrightness*
                    outClientTargetProperty) = 0;
    [[nodiscard]] virtual hal::Error getDisplayDecorationSupport(
            std::optional<aidl::android::hardware::graphics::common::DisplayDecorationSupport>*
                    support) = 0;
    [[nodiscard]] virtual hal::Error setIdleTimerEnabled(std::chrono::milliseconds timeout) = 0;
    [[nodiscard]] virtual hal::Error getPhysicalDisplayOrientation(
            Hwc2::AidlTransform* outTransform) const = 0;
};

namespace impl {

class Layer;

class Display : public HWC2::Display {
public:
    Display(android::Hwc2::Composer&,
            const std::unordered_set<aidl::android::hardware::graphics::composer3::Capability>&,
            hal::HWDisplayId, hal::DisplayType);
    ~Display() override;

    // Required by HWC2
    hal::Error acceptChanges() override;
    base::expected<std::shared_ptr<HWC2::Layer>, hal::Error> createLayer() override;
    hal::Error getChangedCompositionTypes(
            std::unordered_map<HWC2::Layer*,
                               aidl::android::hardware::graphics::composer3::Composition>* outTypes)
            override;
    hal::Error getColorModes(std::vector<hal::ColorMode>* outModes) const override;
    // Returns a bitmask which contains HdrMetadata::Type::*.
    int32_t getSupportedPerFrameMetadata() const override;
    hal::Error getRenderIntents(hal::ColorMode colorMode,
                                std::vector<hal::RenderIntent>* outRenderIntents) const override;
    hal::Error getDataspaceSaturationMatrix(hal::Dataspace, android::mat4* outMatrix) override;

    hal::Error getName(std::string* outName) const override;
    hal::Error getRequests(
            hal::DisplayRequest* outDisplayRequests,
            std::unordered_map<HWC2::Layer*, hal::LayerRequest>* outLayerRequests) override;
    hal::Error getConnectionType(ui::DisplayConnectionType*) const override;
    hal::Error supportsDoze(bool* outSupport) const override EXCLUDES(mDisplayCapabilitiesMutex);
    hal::Error getHdrCapabilities(android::HdrCapabilities* outCapabilities) const override;
    hal::Error getDisplayedContentSamplingAttributes(hal::PixelFormat* outFormat,
                                                     hal::Dataspace* outDataspace,
                                                     uint8_t* outComponentMask) const override;
    hal::Error setDisplayContentSamplingEnabled(bool enabled, uint8_t componentMask,
                                                uint64_t maxFrames) const override;
    hal::Error getDisplayedContentSample(uint64_t maxFrames, uint64_t timestamp,
                                         android::DisplayedFrameStats* outStats) const override;
    hal::Error getReleaseFences(std::unordered_map<HWC2::Layer*, android::sp<android::Fence>>*
                                        outFences) const override;
    hal::Error present(android::sp<android::Fence>* outPresentFence) override;
    hal::Error setClientTarget(uint32_t slot, const android::sp<android::GraphicBuffer>& target,
                               const android::sp<android::Fence>& acquireFence,
                               hal::Dataspace dataspace) override;
    hal::Error setColorMode(hal::ColorMode, hal::RenderIntent) override;
    hal::Error setColorTransform(const android::mat4& matrix) override;
    hal::Error setOutputBuffer(const android::sp<android::GraphicBuffer>&,
                               const android::sp<android::Fence>& releaseFence) override;
    hal::Error setPowerMode(hal::PowerMode) override;
    hal::Error setVsyncEnabled(hal::Vsync enabled) override;
    hal::Error validate(nsecs_t expectedPresentTime, uint32_t* outNumTypes,
                        uint32_t* outNumRequests) override;
    hal::Error presentOrValidate(nsecs_t expectedPresentTime, uint32_t* outNumTypes,
                                 uint32_t* outNumRequests,
                                 android::sp<android::Fence>* outPresentFence,
                                 uint32_t* state) override;
    ftl::Future<hal::Error> setDisplayBrightness(
            float brightness, float brightnessNits,
            const Hwc2::Composer::DisplayBrightnessOptions& options) override;
    hal::Error setActiveConfigWithConstraints(hal::HWConfigId configId,
                                              const hal::VsyncPeriodChangeConstraints& constraints,
                                              hal::VsyncPeriodChangeTimeline* outTimeline) override;
    hal::Error setBootDisplayConfig(hal::HWConfigId configId) override;
    hal::Error clearBootDisplayConfig() override;
    hal::Error getPreferredBootDisplayConfig(hal::HWConfigId* configId) const override;
    hal::Error setAutoLowLatencyMode(bool on) override;
    hal::Error getSupportedContentTypes(
            std::vector<hal::ContentType>* outSupportedContentTypes) const override;
    hal::Error setContentType(hal::ContentType) override;
    hal::Error getClientTargetProperty(
            aidl::android::hardware::graphics::composer3::ClientTargetPropertyWithBrightness*
                    outClientTargetProperty) override;
    hal::Error getDisplayDecorationSupport(
            std::optional<aidl::android::hardware::graphics::common::DisplayDecorationSupport>*
                    support) override;
    hal::Error setIdleTimerEnabled(std::chrono::milliseconds timeout) override;

    // Other Display methods
    hal::HWDisplayId getId() const override { return mId; }
    bool isConnected() const override { return mIsConnected; }
    void setConnected(bool connected) override; // For use by Device only
    bool hasCapability(aidl::android::hardware::graphics::composer3::DisplayCapability)
            const override EXCLUDES(mDisplayCapabilitiesMutex);
    bool isVsyncPeriodSwitchSupported() const override;
    bool hasDisplayIdleTimerCapability() const override;
    void onLayerDestroyed(hal::HWLayerId layerId) override;
    hal::Error getPhysicalDisplayOrientation(Hwc2::AidlTransform* outTransform) const override;

private:

    // This may fail (and return a null pointer) if no layer with this ID exists
    // on this display
    std::shared_ptr<HWC2::Layer> getLayerById(hal::HWLayerId id) const;

    friend android::TestableSurfaceFlinger;

    // Member variables

    // These are references to data owned by HWC2::Device, which will outlive
    // this HWC2::Display, so these references are guaranteed to be valid for
    // the lifetime of this object.
    android::Hwc2::Composer& mComposer;
    const std::unordered_set<aidl::android::hardware::graphics::composer3::Capability>&
            mCapabilities;

    const hal::HWDisplayId mId;
    hal::DisplayType mType;
    bool mIsConnected = false;

    using Layers = std::unordered_map<hal::HWLayerId, std::weak_ptr<HWC2::impl::Layer>>;
    Layers mLayers;

    mutable std::mutex mDisplayCapabilitiesMutex;
    std::once_flag mDisplayCapabilityQueryFlag;
    std::optional<
            std::unordered_set<aidl::android::hardware::graphics::composer3::DisplayCapability>>
            mDisplayCapabilities GUARDED_BY(mDisplayCapabilitiesMutex);
};

} // namespace impl

class Layer {
public:
    virtual ~Layer();

    virtual hal::HWLayerId getId() const = 0;

    [[nodiscard]] virtual hal::Error setCursorPosition(int32_t x, int32_t y) = 0;
    [[nodiscard]] virtual hal::Error setBuffer(uint32_t slot,
                                               const android::sp<android::GraphicBuffer>& buffer,
                                               const android::sp<android::Fence>& acquireFence) = 0;
    [[nodiscard]] virtual hal::Error setSurfaceDamage(const android::Region& damage) = 0;

    [[nodiscard]] virtual hal::Error setBlendMode(hal::BlendMode mode) = 0;
    [[nodiscard]] virtual hal::Error setColor(
            aidl::android::hardware::graphics::composer3::Color color) = 0;
    [[nodiscard]] virtual hal::Error setCompositionType(
            aidl::android::hardware::graphics::composer3::Composition type) = 0;
    [[nodiscard]] virtual hal::Error setDataspace(hal::Dataspace dataspace) = 0;
    [[nodiscard]] virtual hal::Error setPerFrameMetadata(const int32_t supportedPerFrameMetadata,
                                                         const android::HdrMetadata& metadata) = 0;
    [[nodiscard]] virtual hal::Error setDisplayFrame(const android::Rect& frame) = 0;
    [[nodiscard]] virtual hal::Error setPlaneAlpha(float alpha) = 0;
    [[nodiscard]] virtual hal::Error setSidebandStream(const native_handle_t* stream) = 0;
    [[nodiscard]] virtual hal::Error setSourceCrop(const android::FloatRect& crop) = 0;
    [[nodiscard]] virtual hal::Error setTransform(hal::Transform transform) = 0;
    [[nodiscard]] virtual hal::Error setVisibleRegion(const android::Region& region) = 0;
    [[nodiscard]] virtual hal::Error setZOrder(uint32_t z) = 0;

    // Composer HAL 2.3
    [[nodiscard]] virtual hal::Error setColorTransform(const android::mat4& matrix) = 0;

    // Composer HAL 2.4
    [[nodiscard]] virtual hal::Error setLayerGenericMetadata(const std::string& name,
                                                             bool mandatory,
                                                             const std::vector<uint8_t>& value) = 0;

    // AIDL HAL
    [[nodiscard]] virtual hal::Error setBrightness(float brightness) = 0;
    [[nodiscard]] virtual hal::Error setBlockingRegion(const android::Region& region) = 0;
};

namespace impl {

// Convenience C++ class to access per layer functions directly.

class Layer : public HWC2::Layer {
public:
    Layer(android::Hwc2::Composer& composer,
          const std::unordered_set<aidl::android::hardware::graphics::composer3::Capability>&
                  capabilities,
          HWC2::Display& display, hal::HWLayerId layerId);
    ~Layer() override;

    void onOwningDisplayDestroyed();

    hal::HWLayerId getId() const override { return mId; }

    hal::Error setCursorPosition(int32_t x, int32_t y) override;
    hal::Error setBuffer(uint32_t slot, const android::sp<android::GraphicBuffer>& buffer,
                         const android::sp<android::Fence>& acquireFence) override;
    hal::Error setSurfaceDamage(const android::Region& damage) override;

    hal::Error setBlendMode(hal::BlendMode mode) override;
    hal::Error setColor(aidl::android::hardware::graphics::composer3::Color color) override;
    hal::Error setCompositionType(
            aidl::android::hardware::graphics::composer3::Composition type) override;
    hal::Error setDataspace(hal::Dataspace dataspace) override;
    hal::Error setPerFrameMetadata(const int32_t supportedPerFrameMetadata,
                                   const android::HdrMetadata& metadata) override;
    hal::Error setDisplayFrame(const android::Rect& frame) override;
    hal::Error setPlaneAlpha(float alpha) override;
    hal::Error setSidebandStream(const native_handle_t* stream) override;
    hal::Error setSourceCrop(const android::FloatRect& crop) override;
    hal::Error setTransform(hal::Transform transform) override;
    hal::Error setVisibleRegion(const android::Region& region) override;
    hal::Error setZOrder(uint32_t z) override;

    // Composer HAL 2.3
    hal::Error setColorTransform(const android::mat4& matrix) override;

    // Composer HAL 2.4
    hal::Error setLayerGenericMetadata(const std::string& name, bool mandatory,
                                       const std::vector<uint8_t>& value) override;

    // AIDL HAL
    hal::Error setBrightness(float brightness) override;
    hal::Error setBlockingRegion(const android::Region& region) override;

private:
    // These are references to data owned by HWC2::Device, which will outlive
    // this HWC2::Layer, so these references are guaranteed to be valid for
    // the lifetime of this object.
    android::Hwc2::Composer& mComposer;
    const std::unordered_set<aidl::android::hardware::graphics::composer3::Capability>&
            mCapabilities;

    HWC2::Display* mDisplay;
    hal::HWLayerId mId;

    // Cached HWC2 data, to ensure the same commands aren't sent to the HWC
    // multiple times.
    android::Region mVisibleRegion = android::Region::INVALID_REGION;
    android::Region mDamageRegion = android::Region::INVALID_REGION;
    android::Region mBlockingRegion = android::Region::INVALID_REGION;
    hal::Dataspace mDataSpace = hal::Dataspace::UNKNOWN;
    android::HdrMetadata mHdrMetadata;
    android::mat4 mColorMatrix;
    uint32_t mBufferSlot;
};

} // namespace impl
} // namespace HWC2
} // namespace android
