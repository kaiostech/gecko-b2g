/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CreateFileTask.h"

#include <algorithm>

#include "mozilla/Preferences.h"
#include "mozilla/dom/FileBlobImpl.h"
#include "mozilla/dom/FileSystemBase.h"
#include "mozilla/dom/FileSystemUtils.h"
#include "mozilla/dom/IPCBlobUtils.h"
#include "mozilla/dom/PFileSystemParams.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "nsIFile.h"
#include "nsNetUtil.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsString.h"

#define GET_PERMISSION_ACCESS_TYPE(aAccess)            \
  if (mReplace) {                                      \
    aAccess.AssignLiteral(DIRECTORY_WRITE_PERMISSION); \
    return;                                            \
  }                                                    \
  aAccess.AssignLiteral(DIRECTORY_CREATE_PERMISSION);

namespace mozilla {
namespace dom {

/**
 *CreateFileTaskChild
 */

/* static */ already_AddRefed<CreateFileTaskChild> CreateFileTaskChild::Create(
    FileSystemBase* aFileSystem, nsIFile* aTargetPath, Blob* aBlobData,
    nsTArray<uint8_t>& aArrayData, bool aReplace, ErrorResult& aRv) {
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  MOZ_ASSERT(aFileSystem);

  nsCOMPtr<nsIGlobalObject> globalObject = aFileSystem->GetParentObject();
  if (NS_WARN_IF(!globalObject)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<CreateFileTaskChild> task =
      new CreateFileTaskChild(globalObject, aFileSystem, aTargetPath, aReplace);

  // aTargetPath can be null. In this case SetError will be called.

  if (aBlobData) {
    task->mBlobImpl = aBlobData->Impl();
  }

  task->mArrayData.SwapElements(aArrayData);

  task->mPromise = Promise::Create(globalObject, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  return task.forget();
}

CreateFileTaskChild::CreateFileTaskChild(nsIGlobalObject* aGlobalObject,
                                         FileSystemBase* aFileSystem,
                                         nsIFile* aTargetPath, bool aReplace)
    : FileSystemTaskChildBase(aGlobalObject, aFileSystem),
      mTargetPath(aTargetPath),
      mReplace(aReplace) {
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  MOZ_ASSERT(aFileSystem);
}

CreateFileTaskChild::~CreateFileTaskChild() { MOZ_ASSERT(NS_IsMainThread()); }

already_AddRefed<Promise> CreateFileTaskChild::GetPromise() {
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  return RefPtr<Promise>(mPromise).forget();
}

FileSystemParams CreateFileTaskChild::GetRequestParams(
    const nsString& aSerializedDOMPath, ErrorResult& aRv) const {
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  FileSystemCreateFileParams param;
  param.filesystem() = aSerializedDOMPath;

  aRv = mTargetPath->GetPath(param.realPath());
  if (NS_WARN_IF(aRv.Failed())) {
    return param;
  }

  param.replace() = mReplace;
  if (mBlobImpl) {
    IPCBlob ipcBlob;
    nsresult rv = IPCBlobUtils::Serialize(mBlobImpl, ipcBlob);
    if (NS_SUCCEEDED(rv)) {
      param.data() = ipcBlob;
    }
  } else {
    param.data() = mArrayData;
  }
  return param;
}

void CreateFileTaskChild::SetSuccessRequestResult(
    const FileSystemResponseValue& aValue, ErrorResult& aRv) {
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");

  const FileSystemFileResponse& r = aValue.get_FileSystemFileResponse();

  mBlobImpl = IPCBlobUtils::Deserialize(r.blob());
  MOZ_ASSERT(mBlobImpl);
}

void CreateFileTaskChild::HandlerCallback() {
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");

  if (mFileSystem->IsShutdown()) {
    mPromise = nullptr;
    return;
  }

  if (HasError()) {
    mPromise->MaybeReject(mErrorValue);
    mPromise = nullptr;
    return;
  }

  RefPtr<File> file = File::Create(mFileSystem->GetParentObject(), mBlobImpl);
  mPromise->MaybeResolve(file);

  mBlobImpl = nullptr;
  mPromise = nullptr;
}

void CreateFileTaskChild::GetPermissionAccessType(nsCString& aAccess) const {
    GET_PERMISSION_ACCESS_TYPE(aAccess)}

/**
 * CreateFileTaskParent
 */

uint32_t CreateFileTaskParent::sOutputBufferSize = 0;

/* static */ already_AddRefed<CreateFileTaskParent>
CreateFileTaskParent::Create(FileSystemBase* aFileSystem,
                             const FileSystemCreateFileParams& aParam,
                             FileSystemRequestParent* aParent,
                             ErrorResult& aRv) {
  MOZ_ASSERT(XRE_IsParentProcess(), "Only call from parent process!");
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aFileSystem);

  RefPtr<CreateFileTaskParent> task =
      new CreateFileTaskParent(aFileSystem, aParam, aParent);

  aRv = NS_NewLocalFile(aParam.realPath(), true,
                        getter_AddRefs(task->mTargetPath));
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  task->mReplace = aParam.replace();

  const FileSystemFileDataValue& data = aParam.data();

  if (data.type() == FileSystemFileDataValue::TArrayOfuint8_t) {
    task->mArrayData = data.get_ArrayOfuint8_t();
    return task.forget();
  }

  task->mBlobImpl = IPCBlobUtils::Deserialize(data);
  MOZ_ASSERT(task->mBlobImpl, "blobData should not be null.");

  return task.forget();
}

CreateFileTaskParent::CreateFileTaskParent(
    FileSystemBase* aFileSystem, const FileSystemCreateFileParams& aParam,
    FileSystemRequestParent* aParent)
    : FileSystemTaskParentBase(aFileSystem, aParam, aParent), mReplace(false) {
  MOZ_ASSERT(XRE_IsParentProcess(), "Only call from parent process!");
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aFileSystem);
}

FileSystemResponseValue CreateFileTaskParent::GetSuccessRequestResult(
    ErrorResult& aRv) const {
  AssertIsOnBackgroundThread();

  RefPtr<BlobImpl> blobImpl = new FileBlobImpl(mTargetPath);
  IPCBlob ipcBlob;
  nsresult rv =
      IPCBlobUtils::Serialize(blobImpl, ipcBlob);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return FileSystemFileResponse();
  }

  return FileSystemFileResponse(ipcBlob);
}

nsresult CreateFileTaskParent::IOWork() {
  class MOZ_RAII AutoClose final {
   public:
    explicit AutoClose(nsIOutputStream* aStream) : mStream(aStream) {
      MOZ_ASSERT(aStream);
    }

    ~AutoClose() { mStream->Close(); }

   private:
    nsCOMPtr<nsIOutputStream> mStream;
  };

  MOZ_ASSERT(XRE_IsParentProcess(), "Only call from parent process!");
  MOZ_ASSERT(!NS_IsMainThread(), "Only call on worker thread!");

  if (mFileSystem->IsShutdown()) {
    return NS_ERROR_FAILURE;
  }

  if (!mFileSystem->IsSafeFile(mTargetPath)) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  bool exists = false;
  nsresult rv = mTargetPath->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (exists) {
    bool isFile = false;
    rv = mTargetPath->IsFile(&isFile);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!isFile) {
      return NS_ERROR_DOM_FILESYSTEM_TYPE_MISMATCH_ERR;
    }

    if (!mReplace) {
      return NS_ERROR_DOM_FILESYSTEM_PATH_EXISTS_ERR;
    }

    // Remove the old file before creating.
    rv = mTargetPath->Remove(false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  rv = mTargetPath->Create(nsIFile::NORMAL_FILE_TYPE, 0600);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIOutputStream> outputStream;
  rv = NS_NewLocalFileOutputStream(getter_AddRefs(outputStream), mTargetPath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  AutoClose acOutputStream(outputStream);
  MOZ_ASSERT(sOutputBufferSize);

  nsCOMPtr<nsIOutputStream> bufferedOutputStream;
  rv = NS_NewBufferedOutputStream(getter_AddRefs(bufferedOutputStream),
                                  outputStream.forget(), sOutputBufferSize);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  AutoClose acBufferedOutputStream(bufferedOutputStream);

  // Write the file content from blob data.
  if (mBlobImpl) {
    ErrorResult error;
    nsCOMPtr<nsIInputStream> blobStream;
    mBlobImpl->CreateInputStream(getter_AddRefs(blobStream), error);
    if (NS_WARN_IF(error.Failed())) {
      return error.StealNSResult();
    }

    uint64_t bufSize = 0;
    rv = blobStream->Available(&bufSize);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    while (bufSize && !mFileSystem->IsShutdown()) {
      uint32_t written = 0;
      uint32_t writeSize = bufSize < UINT32_MAX ? bufSize : UINT32_MAX;
      rv = bufferedOutputStream->WriteFrom(blobStream, writeSize, &written);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      bufSize -= written;
    }

    blobStream->Close();

    if (mFileSystem->IsShutdown()) {
      return NS_ERROR_FAILURE;
    }

    return NS_OK;
  }

  // Write file content from array data.

  uint32_t written;
  rv = bufferedOutputStream->Write(
      reinterpret_cast<char*>(mArrayData.Elements()), mArrayData.Length(),
      &written);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (mArrayData.Length() != written) {
    return NS_ERROR_DOM_FILESYSTEM_UNKNOWN_ERR;
  }

  return NS_OK;
}

nsresult CreateFileTaskParent::MainThreadWork() {
  MOZ_ASSERT(NS_IsMainThread());

  if (!sOutputBufferSize) {
    sOutputBufferSize = mozilla::Preferences::GetUint(
        "dom.filesystem.outputBufferSize", 4096 * 4);
  }

  return FileSystemTaskParentBase::MainThreadWork();
}

nsresult CreateFileTaskParent::GetTargetPath(nsAString& aPath) const {
  return mTargetPath->GetPath(aPath);
}

void CreateFileTaskParent::GetPermissionAccessType(nsCString& aAccess) const {
  GET_PERMISSION_ACCESS_TYPE(aAccess)
}

}  // namespace dom
}  // namespace mozilla
