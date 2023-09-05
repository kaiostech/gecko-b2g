/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SandboxOpenedFiles.h"

#include <errno.h>
#include <unistd.h>

#include <utility>

#include "SandboxLogging.h"
#include "mozilla/SandboxInfo.h"

namespace mozilla {

// The default move constructor almost works, but Atomic isn't
// move-constructable and the fd needs some special handling.
SandboxOpenedFile::SandboxOpenedFile(SandboxOpenedFile&& aMoved)
    : mPath(std::move(aMoved.mPath)),
      mMaybeFd(aMoved.TakeDesc()),
      mDup(aMoved.mDup),
      mExpectError(aMoved.mExpectError),
      mFlags(aMoved.mFlags) {}

SandboxOpenedFile::SandboxOpenedFile(const char* aPath, Dup aDup, int aFlags)
    : mPath(aPath),
      mDup(aDup == Dup::YES),
      mExpectError(false),
      mFlags(aFlags & O_ACCMODE) {
  MOZ_ASSERT(aPath[0] == '/', "path should be absolute");

  int fd = open(aPath, mFlags | O_CLOEXEC);
  if (fd < 0) {
    mExpectError = true;
  }
  mMaybeFd = fd;
}

SandboxOpenedFile::SandboxOpenedFile(const char* aPath, Error)
    : mPath(aPath), mMaybeFd(-1), mDup(false), mExpectError(true) {}

int SandboxOpenedFile::GetDesc(int aFlags) const {
  int accessMode = aFlags & O_ACCMODE;
  if ((accessMode & mFlags) != accessMode) {
    SANDBOX_LOG("non-read-only open of file %s attempted (flags=0%o)",
                Path(), aFlags);
    return -1;
  }

  int fd;
  if (mDup) {
    fd = mMaybeFd;
    if (fd >= 0) {
      fd = dup(fd);
      if (fd < 0) {
        SANDBOX_LOG_ERRNO("dup");
      }
    }
  } else {
    fd = TakeDesc();
  }
  if (fd < 0 && !mExpectError) {
    SANDBOX_LOG("unexpected multiple open of file %s", Path());
  }
  return fd;
}

SandboxOpenedFile::~SandboxOpenedFile() {
  int fd = TakeDesc();
  if (fd >= 0) {
    close(fd);
  }
}

int SandboxOpenedFiles::GetDesc(const char* aPath, int aFlags) const {
  for (const auto& file : mFiles) {
    if (strcmp(file.Path(), aPath) == 0) {
      return file.GetDesc(aFlags);
    }
  }
  if (SandboxInfo::Get().Test(SandboxInfo::kVerbose)) {
    SANDBOX_LOG("OpenedFiles denied to open file %s", aPath);
  }
  return -1;
}

}  // namespace mozilla
