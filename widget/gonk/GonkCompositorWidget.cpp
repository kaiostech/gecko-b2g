/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GonkCompositorWidget.h"
#include "nsWindow.h"
#include "ScreenHelperGonk.h"

namespace mozilla {
namespace widget {

GonkCompositorWidget::GonkCompositorWidget(
    const layers::CompositorOptions& aOptions, nsBaseWidget* aWidget)
    : InProcessCompositorWidget(aOptions, aWidget) {}

EGLNativeWindowType GonkCompositorWidget::GetEGLNativeWindow() {
  return (EGLNativeWindowType)mWidget->GetNativeData(NS_NATIVE_WINDOW);
}

int32_t GonkCompositorWidget::GetNativeSurfaceFormat() {
  nsWindow* window = static_cast<nsWindow*>(mWidget);
  return window->GetScreen()->GetSurfaceFormat();
}

layers::Composer2D* GonkCompositorWidget::GetComposer2D() {
  nsWindow* window = static_cast<nsWindow*>(mWidget);
  return window->GetComposer2D();
}

}  // namespace widget
}  // namespace mozilla
