/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef IPC_ShadowLayerUtils_h
#define IPC_ShadowLayerUtils_h

#include "ipc/EnumSerializer.h"
#include "ipc/IPCMessageUtils.h"
#include "GLContextTypes.h"
#include "SurfaceDescriptor.h"
#include "SurfaceTypes.h"
#include "mozilla/WidgetUtils.h"

#if defined(MOZ_WIDGET_GONK)
#  include "mozilla/layers/ShadowLayerUtilsGralloc.h"
#else
namespace mozilla {
namespace layers {
struct MagicGrallocBufferHandle {
  bool operator==(const MagicGrallocBufferHandle&) const { return false; }
};

struct GrallocBufferRef {
  bool operator==(const GrallocBufferRef&) const { return false; }
};
}  // namespace layers
}  // namespace mozilla
#endif

namespace IPC {

#if !defined(MOZ_HAVE_SURFACEDESCRIPTORGRALLOC)
template <>
struct ParamTraits<mozilla::layers::MagicGrallocBufferHandle> {
  typedef mozilla::layers::MagicGrallocBufferHandle paramType;
  static void Write(MessageWriter*, const paramType&) {}
  static bool Read(const MessageReader*, paramType*) {
    return false;
  }
};

template <>
struct ParamTraits<mozilla::layers::GrallocBufferRef> {
  typedef mozilla::layers::GrallocBufferRef paramType;
  static void Write(MessageWriter*, const paramType&) {}
  static bool Read(const MessageReader*, paramType*) {
    return false;
  }
};
#endif  // !defined(MOZ_HAVE_XSURFACEDESCRIPTORGRALLOC)

template <>
struct ParamTraits<mozilla::ScreenRotation>
    : public ContiguousEnumSerializer<mozilla::ScreenRotation,
                                      mozilla::ROTATION_0,
                                      mozilla::ROTATION_COUNT> {};

}  // namespace IPC

#endif  // IPC_ShadowLayerUtils_h
