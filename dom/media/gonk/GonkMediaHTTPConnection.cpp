/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GonkMediaHTTPConnection.h"

#include <binder/MemoryDealer.h>
#include <binder/Parcel.h>
#include <binder/Status.h>
#include "MediaResource.h"

namespace android {

using android::binder::Status;
using mozilla::MediaResource;

/** MUST stay in sync with IMediaHTTPConnection.cpp */

enum {
  CONNECT = IBinder::FIRST_CALL_TRANSACTION,
  DISCONNECT,
  READ_AT,
  GET_SIZE,
  GET_MIME_TYPE,
  GET_URI
};

status_t BnMediaHTTPConnection::onTransact(uint32_t code, const Parcel& data,
                                           Parcel* reply, uint32_t flags) {
  switch (code) {
    case CONNECT: {
      CHECK_INTERFACE(IMediaHTTPConnection, data, reply);
      String8 uri(data.readString16());

      // Deserialize headers from String8 into KeyedVector<String8, String8>.
      String8 lines(data.readString16());
      KeyedVector<String8, String8> headers;
      ssize_t start = 0, end;
      while ((end = lines.find("\r\n", start)) >= 0) {
        String8 line(lines.c_str() + start, end - start);
        ssize_t idx = line.find(": ");
        if (idx >= 0) {
          String8 key(line.c_str(), idx);
          String8 value(line.c_str() + idx + 2, line.length() - idx - 2);
          headers.add(key, value);
        }
        start = end + 2;
      }

      bool success = connect(uri.c_str(), &headers);
      if (!success) {
        Status::fromServiceSpecificError(UNKNOWN_ERROR).writeToParcel(reply);
        return NO_ERROR;
      }
      reply->writeNoException();
      reply->writeStrongBinder(IInterface::asBinder(getIMemory()));
      return NO_ERROR;
    } break;
    case DISCONNECT: {
      CHECK_INTERFACE(IMediaHTTPConnection, data, reply);
      disconnect();
      return NO_ERROR;
    } break;
    case READ_AT: {
      CHECK_INTERFACE(IMediaHTTPConnection, data, reply);
      sp<IMemory> memory = getIMemory();
      if (!memory) {
        Status::fromServiceSpecificError(UNKNOWN_ERROR).writeToParcel(reply);
        return NO_ERROR;
      }
      off64_t offset = data.readInt64();
      size_t size = data.readInt32();
      if (size > memory->size()) {
        size = memory->size();
      }
#if ANDROID_VERSION == 30
      ssize_t len = readAt(offset, memory->unsecurePointer(), size);
#else
      ssize_t len = readAt(offset, memory->pointer(), size);
#endif
      reply->writeNoException();
      reply->writeInt32(len);
      return NO_ERROR;
    } break;
    case GET_SIZE: {
      CHECK_INTERFACE(IMediaHTTPConnection, data, reply);
      off64_t size = getSize();
      reply->writeNoException();
      reply->writeInt64(size);
      return NO_ERROR;
    } break;
    case GET_MIME_TYPE: {
      CHECK_INTERFACE(IMediaHTTPConnection, data, reply);
      String8 mimeType;
      status_t err = getMIMEType(&mimeType);
      if (err != OK) {
        Status::fromServiceSpecificError(UNKNOWN_ERROR).writeToParcel(reply);
        return NO_ERROR;
      }
      reply->writeNoException();
      reply->writeString16(String16(mimeType));
      return NO_ERROR;
    } break;
    case GET_URI: {
      CHECK_INTERFACE(IMediaHTTPConnection, data, reply);
      String8 uri;
      status_t err = getUri(&uri);
      if (err != OK) {
        Status::fromServiceSpecificError(UNKNOWN_ERROR).writeToParcel(reply);
        return NO_ERROR;
      }
      reply->writeNoException();
      reply->writeString16(String16(uri));
      return NO_ERROR;
    } break;
    default:
      return BBinder::onTransact(code, data, reply, flags);
  }
}

GonkMediaHTTPConnection::GonkMediaHTTPConnection(RefPtr<MediaResource> resource,
                                                 const char* resourceURI,
                                                 const char* resourceMIMEType)
    : mResource(resource),
      mResourceURI(resourceURI),
      mResourceMIMEType(resourceMIMEType) {}

bool GonkMediaHTTPConnection::connect(
    const char* uri, const KeyedVector<String8, String8>* headers) {
  if (mResourceURI != uri) {
    return false;
  }

  if (!mMemory) {
    sp<MemoryDealer> memoryDealer =
        new MemoryDealer(kBufferSize, "GonkMediaHTTPConnection");
    mMemory = memoryDealer->allocate(kBufferSize);
    if (mMemory.get() == nullptr) {
      return false;
    }
  }
  return true;
}

void GonkMediaHTTPConnection::disconnect() {}

ssize_t GonkMediaHTTPConnection::readAt(off64_t offset, void* data,
                                        size_t size) {
  uint32_t readSize = 0;
  nsresult err =
      mResource->ReadAt(offset, static_cast<char*>(data), size, &readSize);
  if (NS_FAILED(err)) {
    return -1;
  }
  return readSize;
}

off64_t GonkMediaHTTPConnection::getSize() { return mResource->GetLength(); }

status_t GonkMediaHTTPConnection::getMIMEType(String8* mimeType) {
  *mimeType = mResourceMIMEType;
  return OK;
}

status_t GonkMediaHTTPConnection::getUri(String8* uri) {
  *uri = mResourceURI;
  return OK;
}

}  // namespace android
