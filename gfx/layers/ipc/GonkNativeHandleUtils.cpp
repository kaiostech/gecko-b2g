/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GonkNativeHandleUtils.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"

using namespace mozilla::layers;

namespace IPC {

namespace {

class native_handle_Delete {
 public:
  void operator()(native_handle* aNativeHandle) const {
    native_handle_close(aNativeHandle);  // closes file descriptors
    native_handle_delete(aNativeHandle);
  }
};

}  // anonymous namespace

void ParamTraits<GonkNativeHandle>::Write(MessageWriter* aWriter,
                                          const paramType& aParam) {
  GonkNativeHandle handle = aParam;
  MOZ_ASSERT(handle.IsValid());

  RefPtr<GonkNativeHandle::NhObj> nhObj = handle.GetAndResetNhObj();
  native_handle_t* nativeHandle = nhObj->GetAndResetNativeHandle();

  size_t nbytes = nativeHandle->numInts * sizeof(int);
  // TODO: verify this is correct after bug 1525199.
  aWriter->WriteUInt32(nbytes);
  aWriter->WriteBytes((nativeHandle->data + nativeHandle->numFds), nbytes);

  for (size_t i = 0; i < static_cast<size_t>(nativeHandle->numFds); ++i) {
    aWriter->WriteFileHandle(mozilla::UniqueFileHandle(nativeHandle->data[i]));
  }
}

bool ParamTraits<GonkNativeHandle>::Read(MessageReader* aReader,
                                         paramType* aResult) {
  uint32_t nbytes;
  // TODO: verify this is correct after bug 1525199.
  if (!aReader->ReadUInt32(&nbytes)) {
    return false;
  }

  if (nbytes % sizeof(int) != 0) {
    return false;
  }

  size_t numInts = nbytes / sizeof(int);
  size_t numFds = aReader->NumHandles();
  mozilla::UniquePtr<native_handle, native_handle_Delete> nativeHandle(
      native_handle_create(numFds, numInts));
  if (!nativeHandle) {
    return false;
  }

  auto data =
      reinterpret_cast<char*>(nativeHandle->data + nativeHandle->numFds);
  if (!aReader->ReadBytesInto(data, nbytes)) {
    return false;
  }

  for (size_t i = 0; i < numFds; ++i) {
    mozilla::UniqueFileHandle fd;
    if (!aReader->ConsumeFileHandle(&fd)) {
      return false;
    }
    nativeHandle->data[i] = fd.release();
    nativeHandle->numFds = i + 1;  // set number of valid file descriptors
  }

  GonkNativeHandle handle(new GonkNativeHandle::NhObj(nativeHandle.get()));
  handle.TransferToAnother(*aResult);

  mozilla::Unused << nativeHandle.release();

  return true;
}

}  // namespace IPC
