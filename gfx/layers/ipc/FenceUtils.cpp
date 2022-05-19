/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(MOZ_WIDGET_GONK)
#  pragma GCC visibility push(default)
#  include "sync/sync.h"  // for sync_merge
#  pragma GCC visibility pop
#endif

#include "FenceUtils.h"

using namespace mozilla::layers;

namespace IPC {

void ParamTraits<FenceHandle>::Write(MessageWriter* aWriter,
                                     const paramType& aParam) {
  FenceHandle handle = aParam;

  MOZ_ASSERT(handle.IsValid());

#if defined(MOZ_WIDGET_GONK)
  RefPtr<FenceHandle::FdObj> fence = handle.GetAndResetFdObj();
  aWriter->WriteFileHandle(mozilla::UniqueFileHandle(fence->GetAndResetFd()));
#endif
}

bool ParamTraits<FenceHandle>::Read(MessageReader* aReader,
                                    paramType* aResult) {
#if defined(MOZ_WIDGET_GONK)
  mozilla::UniqueFileHandle fd;
  if (aReader->ConsumeFileHandle(&fd)) {
    aResult->Merge(FenceHandle(new FenceHandle::FdObj(fd.release())));
  }
#endif
  return true;
}

}  // namespace IPC

namespace mozilla {
namespace layers {

FenceHandle::FenceHandle() : mFence(new FdObj()) {}

FenceHandle::FenceHandle(FdObj* aFdObj) : mFence(aFdObj) { MOZ_ASSERT(aFdObj); }

void FenceHandle::Merge(const FenceHandle& aFenceHandle) {
#if defined(MOZ_WIDGET_GONK)
  if (!aFenceHandle.IsValid()) {
    return;
  }

  if (!IsValid()) {
    mFence = aFenceHandle.mFence;
  } else {
    int result =
        sync_merge("FenceHandle", mFence->mFd, aFenceHandle.mFence->mFd);
    if (result == -1) {
      mFence = aFenceHandle.mFence;
    } else {
      mFence = new FdObj(result);
    }
  }
#endif
}

void FenceHandle::TransferToAnotherFenceHandle(FenceHandle& aFenceHandle) {
  aFenceHandle.mFence = this->GetAndResetFdObj();
}

already_AddRefed<FenceHandle::FdObj> FenceHandle::GetAndResetFdObj() {
  RefPtr<FdObj> fence = mFence;
  mFence = new FdObj();
  return fence.forget();
}

already_AddRefed<FenceHandle::FdObj> FenceHandle::GetDupFdObj() {
  RefPtr<FdObj> fdObj;
  if (IsValid()) {
    fdObj = new FenceHandle::FdObj(dup(mFence->mFd));
  } else {
    fdObj = new FenceHandle::FdObj();
  }
  return fdObj.forget();
}

}  // namespace layers
}  // namespace mozilla
