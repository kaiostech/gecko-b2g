/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MEDIA_CODEC_PROXY_H
#define MEDIA_CODEC_PROXY_H

#include <nsString.h>
#include <media/MediaCodecBuffer.h>
#include <media/stagefright/MediaCodec.h>
#include <media/stagefright/MetaData.h>
#include <utils/threads.h>

#include "mozilla/media/MediaSystemResourceClient.h"
#include "mozilla/Monitor.h"
#include "mozilla/MozPromise.h"
#include "mozilla/RefPtr.h"

//#define DEBUG_BUFFER_USAGE

namespace android {

class GonkCryptoInfo;

class SimpleMediaBuffer : public RefBase {
 public:
  explicit SimpleMediaBuffer(const sp<MediaCodecBuffer>& aMediaCodecBuffer)
      : mMediaCodecBuffer(aMediaCodecBuffer) {}

  void* Data() const { return mMediaCodecBuffer->data(); }

  size_t Size() const { return mMediaCodecBuffer->size(); }

  MetaDataBase& MetaData() { return mMetaData; }

  void SetGraphicBuffer(const sp<GraphicBuffer>& aGraphicBuffer) {
    mGraphicBuffer = aGraphicBuffer;
  }

  sp<GraphicBuffer> GetGraphicBuffer() { return mGraphicBuffer; }

  void SetManager(const sp<RefBase>& aManager) { mManager = aManager; }

  sp<RefBase> GetManager() { return mManager.promote(); }

 private:
  virtual ~SimpleMediaBuffer() = default;

  sp<MediaCodecBuffer> mMediaCodecBuffer;
  sp<GraphicBuffer> mGraphicBuffer;
  wp<RefBase> mManager;
  MetaDataBase mMetaData;
};

// This class is intended to be a proxy for MediaCodec with codec resource
// management. Basically user can use it like MediaCodec, but need to handle
// the listener when Codec is reserved for Async case. A good example is
// MediaCodecReader.cpp. Another useage is to use configure(), Prepare(),
// Input(), and Output(). It is used in GonkVideoDecoderManager.cpp which
// doesn't need to handle the buffers for codec.
class MediaCodecProxy : public RefBase {
 public:
  typedef mozilla::MozPromise<bool /* aIgnored */, bool /* aIgnored */,
                              /* IsExclusive = */ true>
      CodecPromise;

  enum Capability {
    kEmptyCapability = 0x00000000,
    kCanExposeGraphicBuffer = 0x00000001,
  };

  enum {
    kKeyBufferIndex = 'bfin',
    kKeyGraphicBuffer = 'gbuf',
  };

  // Check whether MediaCodec has been allocated.
  bool allocated() const;

  // Static MediaCodec methods
  // Only support MediaCodec::CreateByType()
  static sp<MediaCodecProxy> CreateByType(sp<ALooper> aLooper,
                                          const char* aMime, bool aEncoder);

  // MediaCodec methods
  status_t configure(const sp<AMessage>& aFormat,
                     const sp<Surface>& aNativeWindow,
                     const sp<ICrypto>& aCrypto, uint32_t aFlags);

  status_t start();

  status_t stop();

  status_t release();

  status_t flush();

  status_t queueInputBuffer(size_t aIndex, size_t aOffset, size_t aSize,
                            int64_t aPresentationTimeUs, uint32_t aFlags,
                            AString* aErrorDetailMessage = nullptr);

  status_t queueSecureInputBuffer(size_t aIndex, size_t aOffset,
                                  const CryptoPlugin::SubSample* aSubSamples,
                                  size_t aNumSubSamples, const uint8_t aKey[16],
                                  const uint8_t aIV[16],
                                  CryptoPlugin::Mode aMode,
                                  const CryptoPlugin::Pattern& aPattern,
                                  int64_t aPresentationTimeUs, uint32_t aFlags,
                                  AString* aErrorDetailMessage = nullptr);

  status_t dequeueInputBuffer(size_t* aIndex, int64_t aTimeoutUs = INT64_C(0));

  status_t dequeueOutputBuffer(size_t* aIndex, size_t* aOffset, size_t* aSize,
                               int64_t* aPresentationTimeUs, uint32_t* aFlags,
                               int64_t aTimeoutUs = INT64_C(0));

  status_t renderOutputBufferAndRelease(size_t aIndex);

  status_t releaseOutputBuffer(size_t aIndex);

  status_t signalEndOfInputStream();

  status_t getOutputFormat(sp<AMessage>* aFormat) const;

  status_t getInputBuffers(Vector<sp<MediaCodecBuffer>>* aBuffers) const;

  status_t getOutputBuffers(Vector<sp<MediaCodecBuffer>>* aBuffers) const;

  // Notification will be posted once there "is something to do", i.e.
  // an input/output buffer has become available, a format change is
  // pending, an error is pending.
  void requestActivityNotification(const sp<AMessage>& aNotify);

  status_t getOutputGraphicBufferFromIndex(size_t aIndex,
                                           sp<GraphicBuffer>* aGraphicBuffer);

  status_t getCapability(uint32_t* aCapability);

  // Utility functions

  // If aData is null, will notify decoder input EOS
  status_t Input(const uint8_t* aData, uint32_t aDataSize,
                 int64_t aTimestampUsecs, uint64_t flags,
                 int64_t aTimeoutUs = 0,
                 const sp<GonkCryptoInfo>& aCryptoInfo = nullptr);
  status_t Output(sp<SimpleMediaBuffer>* aBuffer, int64_t aTimeoutUs);
  bool Prepare();
  void ReleaseMediaResources();
  // This updates mOutputBuffer when receiving INFO_OUTPUT_BUFFERS_CHANGED
  // event.
  bool UpdateOutputBuffers();

  void ReleaseMediaBuffer(const sp<SimpleMediaBuffer>& aBuffer);

  // It allocates audio MediaCodec synchronously.
  bool AllocateAudioMediaCodec();

  // It allocates video MediaCodec asynchronously.
  RefPtr<CodecPromise> AsyncAllocateVideoMediaCodec();

  // Free the OMX codec so others can allocate it.
  void ReleaseMediaCodec();

 protected:
  virtual ~MediaCodecProxy();

 private:
  // Forbidden
  MediaCodecProxy() = delete;
  MediaCodecProxy(const MediaCodecProxy&) = delete;
  const MediaCodecProxy& operator=(const MediaCodecProxy&) = delete;

  // Constructor for MediaCodecProxy::CreateByType
  MediaCodecProxy(sp<ALooper> aLooper, const char* aMime, bool aEncoder);

  // Allocate Codec Resource
  bool allocateCodec();
  // Release Codec Resource
  void releaseCodec();

  // MediaCodec Parameter
  sp<ALooper> mCodecLooper;
  nsCString mCodecMime;
  bool mCodecEncoder;

  // MediaCodec instance
  mutable RWLock mCodecLock;
  sp<MediaCodec> mCodec;

  // MediaCodec buffers to hold input/output data.
  Vector<sp<MediaCodecBuffer>> mInputBuffers;
  Vector<sp<MediaCodecBuffer>> mOutputBuffers;

  // GraphicBuffer buffers pointer to hold output GraphicBuffer data.
  // Use this since there is no sp<GraphicBuffer> mGraphicBuffer in
  // android::MediaBuffer.
  Vector<sp<GraphicBuffer>> mOutputGraphicBuffers;

  // Component name for debugging.
  AString mComponentName;

#ifdef DEBUG_BUFFER_USAGE
  void PrintBufferStats(const char* aFunc = NULL, const int aIndex = -1);
  std::vector<int> mInputBuffersCounter;
  std::vector<int> mOutputBuffersCounter;
#endif
};

}  // namespace android

#endif  // MEDIA_CODEC_PROXY_H
