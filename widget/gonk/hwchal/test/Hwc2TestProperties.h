/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef _HWC2_TEST_PROPERTIES_H
#define _HWC2_TEST_PROPERTIES_H

#include <array>
#include <vector>

#include <HWC2.h>
#include <ComposerHal.h>

#include <ui/GraphicTypes.h>
#include <ui/Region.h>

enum class Hwc2TestCoverage {
  Default = 0,
  Basic,
  Complete,
};

enum class Hwc2TestPropertyName {
  BlendMode = 1,
  BufferArea,
  Color,
  Composition,
  CursorPosition,
  Dataspace,
  DisplayFrame,
  PlaneAlpha,
  SourceCrop,
  SurfaceDamage,
  Transform,
};

typedef struct {
  int32_t width;
  int32_t height;
} Area;

typedef struct {
  uint32_t width;
  uint32_t height;
} UnsignedArea;

class Hwc2TestContainer {
 public:
  virtual ~Hwc2TestContainer() = default;

  /* Resets the container */
  virtual void reset() = 0;

  /* Attempts to advance to the next valid value. Returns true if one can be
   * found */
  virtual bool advance() = 0;

  virtual std::string dump() const = 0;

  /* Returns true if the container supports the given composition type */
  virtual bool isSupported(HWC2::Composition composition) = 0;
};

template <class T>
class Hwc2TestProperty : public Hwc2TestContainer {
 public:
  Hwc2TestProperty(Hwc2TestCoverage coverage,
                   const std::vector<T>& completeList,
                   const std::vector<T>& basicList,
                   const std::vector<T>& defaultList,
                   const std::array<bool, 6>& compositionSupport)
      : Hwc2TestProperty((coverage == Hwc2TestCoverage::Complete) ? completeList
                         : (coverage == Hwc2TestCoverage::Basic)  ? basicList
                                                                  : defaultList,
                         compositionSupport) {}

  Hwc2TestProperty(const std::vector<T>& list,
                   const std::array<bool, 6>& compositionSupport)
      : mList(list), mCompositionSupport(compositionSupport) {}

  void reset() override { mListIdx = 0; }

  bool advance() override {
    if (mListIdx + 1 < mList.size()) {
      mListIdx++;
      updateDependents();
      return true;
    }
    reset();
    updateDependents();
    return false;
  }

  T get() const { return mList.at(mListIdx); }

  virtual bool isSupported(HWC2::Composition composition) {
    return mCompositionSupport.at(int(composition));
  }

 protected:
  /* If a derived class has dependents, override this function */
  virtual void updateDependents() {}

  const std::vector<T>& mList;
  size_t mListIdx = 0;

  const std::array<bool, 6>& mCompositionSupport;
};

class Hwc2TestBuffer;
class Hwc2TestSourceCrop;
class Hwc2TestSurfaceDamage;

class Hwc2TestBufferArea : public Hwc2TestProperty<Area> {
 public:
  Hwc2TestBufferArea(Hwc2TestCoverage coverage, const Area& displayArea);

  std::string dump() const override;

  void setDependent(Hwc2TestBuffer* buffer);
  void setDependent(Hwc2TestSourceCrop* sourceCrop);
  void setDependent(Hwc2TestSurfaceDamage* surfaceDamage);

 protected:
  void update();
  void updateDependents() override;

  const std::vector<float>& mScalars;
  static const std::vector<float> mDefaultScalars;
  static const std::vector<float> mBasicScalars;
  static const std::vector<float> mCompleteScalars;

  Area mDisplayArea;

  Hwc2TestBuffer* mBuffer = nullptr;
  Hwc2TestSourceCrop* mSourceCrop = nullptr;
  Hwc2TestSurfaceDamage* mSurfaceDamage = nullptr;

  std::vector<Area> mBufferAreas;

  static const std::array<bool, 6> mCompositionSupport;
};

class Hwc2TestColor;

class Hwc2TestBlendMode : public Hwc2TestProperty<HWC2::BlendMode> {
 public:
  explicit Hwc2TestBlendMode(Hwc2TestCoverage coverage);

  std::string dump() const override;

  void setDependent(Hwc2TestColor* color);

 protected:
  void updateDependents() override;

  Hwc2TestColor* mColor = nullptr;

  static const std::vector<HWC2::BlendMode> mDefaultBlendModes;
  static const std::vector<HWC2::BlendMode> mBasicBlendModes;
  static const std::vector<HWC2::BlendMode> mCompleteBlendModes;

  static const std::array<bool, 6> mCompositionSupport;
};

class Hwc2TestColor : public Hwc2TestProperty<hwc_color_t> {
 public:
  explicit Hwc2TestColor(Hwc2TestCoverage coverage,
                         HWC2::BlendMode blendMode = HWC2::BlendMode::None);

  std::string dump() const override;

  void updateBlendMode(HWC2::BlendMode blendMode);

 protected:
  void update();

  std::vector<hwc_color_t> mBaseColors;
  static const std::vector<hwc_color_t> mDefaultBaseColors;
  static const std::vector<hwc_color_t> mBasicBaseColors;
  static const std::vector<hwc_color_t> mCompleteBaseColors;

  HWC2::BlendMode mBlendMode;

  std::vector<hwc_color_t> mColors;

  static const std::array<bool, 6> mCompositionSupport;
};

class Hwc2TestComposition : public Hwc2TestProperty<HWC2::Composition> {
 public:
  explicit Hwc2TestComposition(Hwc2TestCoverage coverage);

  std::string dump() const override;

 protected:
  static const std::vector<HWC2::Composition> mDefaultCompositions;
  static const std::vector<HWC2::Composition> mBasicCompositions;
  static const std::vector<HWC2::Composition> mCompleteCompositions;

  static const std::array<bool, 6> mCompositionSupport;
};

class Hwc2TestDataspace : public Hwc2TestProperty<android::ui::Dataspace> {
 public:
  explicit Hwc2TestDataspace(Hwc2TestCoverage coverage);

  std::string dump() const override;

 protected:
  static const std::vector<android::ui::Dataspace> defaultDataspaces;
  static const std::vector<android::ui::Dataspace> basicDataspaces;
  static const std::vector<android::ui::Dataspace> completeDataspaces;

  static const std::array<bool, 6> mCompositionSupport;
};

class Hwc2TestVirtualBuffer;

class Hwc2TestDisplayDimension : public Hwc2TestProperty<UnsignedArea> {
 public:
  explicit Hwc2TestDisplayDimension(Hwc2TestCoverage coverage);

  std::string dump() const;

  void setDependent(Hwc2TestVirtualBuffer* buffer);

 private:
  void updateDependents();

  std::set<Hwc2TestVirtualBuffer*> mBuffers;

  static const std::vector<UnsignedArea> mDefaultDisplayDimensions;
  static const std::vector<UnsignedArea> mBasicDisplayDimensions;
  static const std::vector<UnsignedArea> mCompleteDisplayDimensions;

  static const std::array<bool, 6> mCompositionSupport;
};

class Hwc2TestDisplayFrame : public Hwc2TestProperty<android::Rect> {
 public:
  Hwc2TestDisplayFrame(Hwc2TestCoverage coverage, const Area& displayArea);

  std::string dump() const override;

 protected:
  void update();

  const std::vector<android::FloatRect>& mFrectScalars;
  const static std::vector<android::FloatRect> mDefaultFrectScalars;
  const static std::vector<android::FloatRect> mBasicFrectScalars;
  const static std::vector<android::FloatRect> mCompleteFrectScalars;

  Area mDisplayArea;

  std::vector<android::Rect> mDisplayFrames;

  static const std::array<bool, 6> mCompositionSupport;
};

class Hwc2TestPlaneAlpha : public Hwc2TestProperty<float> {
 public:
  explicit Hwc2TestPlaneAlpha(Hwc2TestCoverage coverage);

  std::string dump() const override;

 protected:
  static const std::vector<float> mDefaultPlaneAlphas;
  static const std::vector<float> mBasicPlaneAlphas;
  static const std::vector<float> mCompletePlaneAlphas;

  static const std::array<bool, 6> mCompositionSupport;
};

class Hwc2TestSourceCrop : public Hwc2TestProperty<android::FloatRect> {
 public:
  explicit Hwc2TestSourceCrop(Hwc2TestCoverage coverage,
                              const Area& bufferArea = {0, 0});

  std::string dump() const override;

  void updateBufferArea(const Area& bufferArea);

 protected:
  void update();

  const std::vector<android::FloatRect>& mFrectScalars;
  const static std::vector<android::FloatRect> mDefaultFrectScalars;
  const static std::vector<android::FloatRect> mBasicFrectScalars;
  const static std::vector<android::FloatRect> mCompleteFrectScalars;

  Area mBufferArea;

  std::vector<android::FloatRect> mSourceCrops;

  static const std::array<bool, 6> mCompositionSupport;
};

class Hwc2TestSurfaceDamage : public Hwc2TestProperty<android::Region> {
 public:
  explicit Hwc2TestSurfaceDamage(Hwc2TestCoverage coverage);
  ~Hwc2TestSurfaceDamage();

  std::string dump() const override;

  void updateBufferArea(const Area& bufferArea);

 protected:
  void update();
  void freeSurfaceDamages();

  const std::vector<std::vector<android::FloatRect>>& mRegionScalars;
  const static std::vector<std::vector<android::FloatRect>>
      mDefaultRegionScalars;
  const static std::vector<std::vector<android::FloatRect>> mBasicRegionScalars;
  const static std::vector<std::vector<android::FloatRect>>
      mCompleteRegionScalars;

  Area mBufferArea = {0, 0};

  std::vector<android::Region> mSurfaceDamages;

  static const std::array<bool, 6> mCompositionSupport;
};

class Hwc2TestTransform : public Hwc2TestProperty<HWC2::Transform> {
 public:
  explicit Hwc2TestTransform(Hwc2TestCoverage coverage);

  std::string dump() const override;

 protected:
  static const std::vector<HWC2::Transform> mDefaultTransforms;
  static const std::vector<HWC2::Transform> mBasicTransforms;
  static const std::vector<HWC2::Transform> mCompleteTransforms;

  static const std::array<bool, 6> mCompositionSupport;
};

class Hwc2TestVisibleRegion {
 public:
  ~Hwc2TestVisibleRegion();

  std::string dump() const;

  void set(const android::Region& visibleRegion);
  android::Region get() const;
  void release();

 protected:
  android::Region mVisibleRegion;
};

#endif /* ifndef _HWC2_TEST_PROPERTIES_H */
