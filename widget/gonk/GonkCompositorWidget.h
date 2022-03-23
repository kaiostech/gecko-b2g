/* -*- Mode: c++; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_GonkCompositorWidget_h
#define mozilla_widget_GonkCompositorWidget_h

#include "GLDefs.h"
#include "mozilla/widget/InProcessCompositorWidget.h"

struct ANativeWindow;

namespace mozilla {

namespace layers {
class Composer2D;
}

namespace widget {

/**
 * GonkCompositorWidget inherits from InProcessCompositorWidget because
 * Android does not support OOP compositing yet. Once it does,
 * GonkCompositorWidget will be made to inherit from CompositorWidget
 * instead.
 */
class GonkCompositorWidget final : public InProcessCompositorWidget {
 public:
  GonkCompositorWidget(const layers::CompositorOptions& aOptions,
                       nsBaseWidget* aWidget);

  GonkCompositorWidget* AsGonk() override { return this; }

  EGLNativeWindowType GetEGLNativeWindow();
  int32_t GetNativeSurfaceFormat();
  layers::Composer2D* GetComposer2D();
};

}  // namespace widget
}  // namespace mozilla

#endif  // mozilla_widget_GonkCompositorWidget_h
