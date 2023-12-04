/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MediaResourceDataSource_h_
#define MediaResourceDataSource_h_

#include <media/DataSource.h>
#include "mozilla/RefPtr.h"

namespace mozilla {

class MediaResource;

class MediaResourceDataSource : public android::DataSource {
 public:
  MediaResourceDataSource(MediaResource* aResource);
  virtual android::status_t initCheck() const override;
  virtual android::status_t getSize(off64_t* size) override;
  virtual ssize_t readAt(off64_t offset, void* data, size_t size) override;

 protected:
  ~MediaResourceDataSource() = default;

 private:
  RefPtr<MediaResource> mResource;
};

}  // namespace mozilla

#endif
