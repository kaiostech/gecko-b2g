/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaCodecProxy.h"
#include <string.h>
#include <binder/IPCThreadState.h>
#include <media/MediaCodecBuffer.h>
#include <media/stagefright/foundation/ABuffer.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/MetaDataBase.h>
#include <media/stagefright/MediaErrors.h>

mozilla::LazyLogModule gMediaCodecProxyLog("MediaCodecProxy");
#undef LOG
#undef LOGE

#define LOG(x, ...)                                      \
  MOZ_LOG(gMediaCodecProxyLog, mozilla::LogLevel::Debug, \
          ("%p " x, this, ##__VA_ARGS__))
#define LOGE(x, ...)                                     \
  MOZ_LOG(gMediaCodecProxyLog, mozilla::LogLevel::Error, \
          ("%p " x, this, ##__VA_ARGS__))

namespace android {

// General Template: MediaCodec::getOutputGraphicBufferFromIndex(...)
template <typename T, bool InterfaceSupported>
struct OutputGraphicBufferStub {
  static status_t GetOutputGraphicBuffer(T* aMediaCodec, size_t aIndex,
                                         sp<GraphicBuffer>* aGraphicBuffer) {
    return ERROR_UNSUPPORTED;
  }
};

// Class Template Specialization:
// MediaCodec::getOutputGraphicBufferFromIndex(...)
template <typename T>
struct OutputGraphicBufferStub<T, true> {
  static status_t GetOutputGraphicBuffer(T* aMediaCodec, size_t aIndex,
                                         sp<GraphicBuffer>* aGraphicBuffer) {
    if (aMediaCodec == nullptr || aGraphicBuffer == nullptr) {
      return BAD_VALUE;
    }
    *aGraphicBuffer = aMediaCodec->getOutputGraphicBufferFromIndex(aIndex);
    return OK;
  }
};

// Wrapper class to handle interface-difference of MediaCodec.
struct MediaCodecInterfaceWrapper {
  typedef int8_t Supported;
  typedef int16_t Unsupported;

  template <typename T>
  static auto TestOutputGraphicBuffer(T* aMediaCodec)
      -> decltype(aMediaCodec->getOutputGraphicBufferFromIndex(0), Supported());

  template <typename T>
  static auto TestOutputGraphicBuffer(...) -> Unsupported;

  // SFINAE: Substitution Failure Is Not An Error
  static const bool OutputGraphicBufferSupported =
      sizeof(TestOutputGraphicBuffer<MediaCodec>(nullptr)) == sizeof(Supported);

  // Class Template Specialization
  static OutputGraphicBufferStub<MediaCodec, OutputGraphicBufferSupported>
      sOutputGraphicBufferStub;

  // Wrapper Function
  static status_t GetOutputGraphicBuffer(MediaCodec* aMediaCodec, size_t aIndex,
                                         sp<GraphicBuffer>* aGraphicBuffer) {
    return sOutputGraphicBufferStub.GetOutputGraphicBuffer(aMediaCodec, aIndex,
                                                           aGraphicBuffer);
  }
};

sp<MediaCodecProxy> MediaCodecProxy::CreateByType(sp<ALooper> aLooper,
                                                  const char* aMime,
                                                  bool aEncoder) {
  sp<MediaCodecProxy> codec = new MediaCodecProxy(aLooper, aMime, aEncoder);
  return codec;
}

MediaCodecProxy::MediaCodecProxy(sp<ALooper> aLooper, const char* aMime,
                                 bool aEncoder)
    : mCodecLooper(aLooper),
      mCodecMime(aMime),
      mCodecEncoder(aEncoder),
      mPromiseMonitor("MediaCodecProxy::mPromiseMonitor") {
  MOZ_ASSERT(mCodecLooper != nullptr, "ALooper should not be nullptr.");
}

MediaCodecProxy::~MediaCodecProxy() { ReleaseMediaCodec(); }

bool MediaCodecProxy::AllocateAudioMediaCodec() {
  if (mResourceClient || mCodec.get()) {
    return false;
  }

  if (strncasecmp(mCodecMime.get(), "audio/", 6) == 0) {
    if (allocateCodec()) {
      return true;
    }
  }
  return false;
}

RefPtr<MediaCodecProxy::CodecPromise>
MediaCodecProxy::AsyncAllocateVideoMediaCodec() {
  if (mResourceClient || mCodec.get()) {
    return CodecPromise::CreateAndReject(true, __func__);
  }

  if (strncasecmp(mCodecMime.get(), "video/", 6) != 0) {
    return CodecPromise::CreateAndReject(true, __func__);
  }
  // request video codec
  mozilla::MediaSystemResourceType type =
      mCodecEncoder ? mozilla::MediaSystemResourceType::VIDEO_ENCODER
                    : mozilla::MediaSystemResourceType::VIDEO_DECODER;
  mResourceClient = new mozilla::MediaSystemResourceClient(type);
  mResourceClient->SetListener(this);

  // MediaCodecProxy::ResourceReserved() will return promise
  // so make sure promise is set before acquire resource to avoid deadlock
  // (if promise doesn't exist, can't reject or resolve. video will just hang
  // there with black screen)
  RefPtr<CodecPromise> p = nullptr;
  {
    mozilla::MonitorAutoLock lock(mPromiseMonitor);
    p = mCodecPromise.Ensure(__func__);
  }

  mResourceClient->Acquire();

  return p;
}

void MediaCodecProxy::ReleaseMediaCodec() {
  // At first, release mResourceClient's resource to prevent a conflict with
  // mResourceClient's callback.
  if (mResourceClient) {
    mResourceClient->ReleaseResource();
    mResourceClient = nullptr;
  }

  mozilla::MonitorAutoLock lock(mPromiseMonitor);
  mCodecPromise.RejectIfExists(true, __func__);
  releaseCodec();
}

bool MediaCodecProxy::allocateCodec() {
  if (mCodecLooper == nullptr) {
    return false;
  }

  // Write Lock for mCodec
  RWLock::AutoWLock awl(mCodecLock);

  // Create MediaCodec
  mCodec =
      MediaCodec::CreateByType(mCodecLooper, mCodecMime.get(), mCodecEncoder);
  if (mCodec == nullptr) {
    return false;
  }

  return true;
}

void MediaCodecProxy::releaseCodec() {
  wp<MediaCodec> codec;

  {
    // Write Lock for mCodec
    RWLock::AutoWLock awl(mCodecLock);

    codec = mCodec;

    // Release MediaCodec
    if (mCodec != nullptr) {
      mCodec->stop();
      mCodec->release();
      mCodec = nullptr;
    }
  }

  while (codec.promote() != nullptr) {
    // this value come from stagefright's AwesomePlayer.
    usleep(1000);
  }

  // Complete all pending Binder ipc transactions
  IPCThreadState::self()->flushCommands();
}

bool MediaCodecProxy::allocated() const {
  // Read Lock for mCodec
  RWLock::AutoRLock arl(mCodecLock);

  return mCodec != nullptr;
}

status_t MediaCodecProxy::configure(const sp<AMessage>& aFormat,
                                    const sp<Surface>& aNativeWindow,
                                    const sp<ICrypto>& aCrypto,
                                    uint32_t aFlags) {
  // Read Lock for mCodec
  RWLock::AutoRLock arl(mCodecLock);

  if (mCodec == nullptr) {
    return NO_INIT;
  }
  return mCodec->configure(aFormat, aNativeWindow, aCrypto, aFlags);
}

status_t MediaCodecProxy::start() {
  // Read Lock for mCodec
  RWLock::AutoRLock arl(mCodecLock);

  if (mCodec == nullptr) {
    return NO_INIT;
  }

  return mCodec->start();
}

status_t MediaCodecProxy::stop() {
  // Read Lock for mCodec
  RWLock::AutoRLock arl(mCodecLock);

  if (mCodec == nullptr) {
    return NO_INIT;
  }
  return mCodec->stop();
}

status_t MediaCodecProxy::release() {
  // Read Lock for mCodec
  RWLock::AutoRLock arl(mCodecLock);

  if (mCodec == nullptr) {
    return NO_INIT;
  }

#ifdef DEBUG_BUFFER_USAGE
  mInputBuffersCounter.clear();
  mOutputBuffersCounter.clear();
#endif

  return mCodec->release();
}

status_t MediaCodecProxy::flush() {
  // Read Lock for mCodec
  RWLock::AutoRLock arl(mCodecLock);

  if (mCodec == nullptr) {
    return NO_INIT;
  }
  return mCodec->flush();
}

status_t MediaCodecProxy::queueInputBuffer(size_t aIndex, size_t aOffset,
                                           size_t aSize,
                                           int64_t aPresentationTimeUs,
                                           uint32_t aFlags,
                                           AString* aErrorDetailMessage) {
  // Read Lock for mCodec
  RWLock::AutoRLock arl(mCodecLock);

  if (mCodec == nullptr) {
    return NO_INIT;
  }

#ifdef DEBUG_BUFFER_USAGE
  mInputBuffersCounter[aIndex]--;
  PrintBufferStats(__func__, aIndex);
#endif

  return mCodec->queueInputBuffer(aIndex, aOffset, aSize, aPresentationTimeUs,
                                  aFlags, aErrorDetailMessage);
}

status_t MediaCodecProxy::queueSecureInputBuffer(
    size_t aIndex, size_t aOffset, const CryptoPlugin::SubSample* aSubSamples,
    size_t aNumSubSamples, const uint8_t aKey[16], const uint8_t aIV[16],
    CryptoPlugin::Mode aMode, const CryptoPlugin::Pattern& aPattern,
    int64_t aPresentationTimeUs, uint32_t aFlags,
    AString* aErrorDetailMessage) {
  // Read Lock for mCodec
  RWLock::AutoRLock arl(mCodecLock);

  if (mCodec == nullptr) {
    return NO_INIT;
  }
  return mCodec->queueSecureInputBuffer(
      aIndex, aOffset, aSubSamples, aNumSubSamples, aKey, aIV, aMode, aPattern,
      aPresentationTimeUs, aFlags, aErrorDetailMessage);
}

status_t MediaCodecProxy::dequeueInputBuffer(size_t* aIndex,
                                             int64_t aTimeoutUs) {
  // Read Lock for mCodec
  RWLock::AutoRLock arl(mCodecLock);

  if (mCodec == nullptr) {
    return NO_INIT;
  }

#ifdef DEBUG_BUFFER_USAGE
  status_t res = mCodec->dequeueInputBuffer(aIndex, aTimeoutUs);
  if (res == OK) {
    mInputBuffersCounter[*aIndex]++;
  }
  PrintBufferStats(__func__, *aIndex);
  return res;
#else
  return mCodec->dequeueInputBuffer(aIndex, aTimeoutUs);
#endif
}

status_t MediaCodecProxy::dequeueOutputBuffer(size_t* aIndex, size_t* aOffset,
                                              size_t* aSize,
                                              int64_t* aPresentationTimeUs,
                                              uint32_t* aFlags,
                                              int64_t aTimeoutUs) {
  // Read Lock for mCodec
  RWLock::AutoRLock arl(mCodecLock);

  if (mCodec == nullptr) {
    LOGE("Cannot dequeueOutputBuffer without MediaCodec.");
    return NO_INIT;
  }

#ifdef DEBUG_BUFFER_USAGE
  status_t res = mCodec->dequeueOutputBuffer(
      aIndex, aOffset, aSize, aPresentationTimeUs, aFlags, aTimeoutUs);
  if (res == OK) {
    mOutputBuffersCounter[*aIndex]++;
  }
  PrintBufferStats(__func__, *aIndex);
  return res;
#else
  return mCodec->dequeueOutputBuffer(aIndex, aOffset, aSize,
                                     aPresentationTimeUs, aFlags, aTimeoutUs);
#endif
}

status_t MediaCodecProxy::renderOutputBufferAndRelease(size_t aIndex) {
  // Read Lock for mCodec
  RWLock::AutoRLock arl(mCodecLock);

  if (mCodec == nullptr) {
    return NO_INIT;
  }
  return mCodec->renderOutputBufferAndRelease(aIndex);
}

status_t MediaCodecProxy::releaseOutputBuffer(size_t aIndex) {
  // Read Lock for mCodec
  RWLock::AutoRLock arl(mCodecLock);

  if (mCodec == nullptr) {
    return NO_INIT;
  }
#ifdef DEBUG_BUFFER_USAGE
  mOutputBuffersCounter[aIndex]--;
  PrintBufferStats(__func__, aIndex);
#endif
  return mCodec->releaseOutputBuffer(aIndex);
}

status_t MediaCodecProxy::signalEndOfInputStream() {
  // Read Lock for mCodec
  RWLock::AutoRLock arl(mCodecLock);

  if (mCodec == nullptr) {
    return NO_INIT;
  }
  return mCodec->signalEndOfInputStream();
}

status_t MediaCodecProxy::getOutputFormat(sp<AMessage>* aFormat) const {
  // Read Lock for mCodec
  RWLock::AutoRLock arl(mCodecLock);

  if (mCodec == nullptr) {
    return NO_INIT;
  }
  return mCodec->getOutputFormat(aFormat);
}

status_t MediaCodecProxy::getInputBuffers(
    Vector<sp<MediaCodecBuffer>>* aBuffers) const {
  // Read Lock for mCodec
  RWLock::AutoRLock arl(mCodecLock);

  if (mCodec == nullptr) {
    return NO_INIT;
  }
  return mCodec->getInputBuffers(aBuffers);
}

status_t MediaCodecProxy::getOutputBuffers(
    Vector<sp<MediaCodecBuffer>>* aBuffers) const {
  // Read Lock for mCodec
  RWLock::AutoRLock arl(mCodecLock);

  if (mCodec == nullptr) {
    return NO_INIT;
  }
  return mCodec->getOutputBuffers(aBuffers);
}

void MediaCodecProxy::requestActivityNotification(const sp<AMessage>& aNotify) {
  // Read Lock for mCodec
  RWLock::AutoRLock arl(mCodecLock);

  if (mCodec == nullptr) {
    return;
  }
  mCodec->requestActivityNotification(aNotify);
}

status_t MediaCodecProxy::getOutputGraphicBufferFromIndex(
    size_t aIndex, sp<GraphicBuffer>* aGraphicBuffer) {
  // Read Lock for mCodec
  RWLock::AutoRLock arl(mCodecLock);

  if (mCodec == nullptr) {
    return NO_INIT;
  }
  return MediaCodecInterfaceWrapper::GetOutputGraphicBuffer(
      mCodec.get(), aIndex, aGraphicBuffer);
}

status_t MediaCodecProxy::getCapability(uint32_t* aCapability) {
  if (aCapability == nullptr) {
    return BAD_VALUE;
  }

  uint32_t capability = kEmptyCapability;

  if (MediaCodecInterfaceWrapper::OutputGraphicBufferSupported) {
    capability |= kCanExposeGraphicBuffer;
  }

  *aCapability = capability;

  return OK;
}

// Called on ImageBridge thread
void MediaCodecProxy::ResourceReserved() {
  LOG("resourceReserved");
  mozilla::MonitorAutoLock lock(mPromiseMonitor);
  // Create MediaCodec
  if (!allocateCodec()) {
    mCodecPromise.RejectIfExists(true, __func__);
    return;
  }
  mCodecPromise.ResolveIfExists(true, __func__);
}

// Called on ImageBridge thread
void MediaCodecProxy::ResourceReserveFailed() {
  LOGE("Resource Reserve Failed");
  mozilla::MonitorAutoLock lock(mPromiseMonitor);
  mCodecPromise.RejectIfExists(true, __func__);
}

bool MediaCodecProxy::Prepare() {
  if (start() != OK) {
    LOGE("Couldn't start MediaCodec");
    return false;
  }
  if (getInputBuffers(&mInputBuffers) != OK) {
    LOGE("Couldn't get input buffers from MediaCodec");
    return false;
  }
  if (getOutputBuffers(&mOutputBuffers) != OK) {
    LOGE("Couldn't get output buffers from MediaCodec");
    return false;
  }

  if (mOutputBuffers.size() > mOutputGraphicBuffers.capacity()) {
    mOutputGraphicBuffers.setCapacity(mOutputBuffers.size());
  }

#ifdef DEBUG_BUFFER_USAGE
  LOG("mInputBuffers.size(): %d", mInputBuffers.size());
  for (uint i = 0; i < mInputBuffers.size(); i++) {
    mInputBuffersCounter.push_back(0);
  }
  LOG("mOutputBuffers.size(): %d", mOutputBuffers.size());
  for (uint j = 0; j < mOutputBuffers.size(); j++) {
    mOutputBuffersCounter.push_back(0);
  }
#endif

  return true;
}

bool MediaCodecProxy::UpdateOutputBuffers() {
  // Read Lock for mCodec
  {
    RWLock::AutoRLock autolock(mCodecLock);
    if (mCodec == nullptr) {
      LOGE("MediaCodec has not been inited from UpdateOutputBuffers");
      return false;
    }
  }

  status_t err = getOutputBuffers(&mOutputBuffers);
  if (err != OK) {
    LOGE("Couldn't update output buffers from MediaCodec");
    return false;
  }
  return true;
}

status_t MediaCodecProxy::Input(const uint8_t* aData, uint32_t aDataSize,
                                int64_t aTimestampUsecs, uint64_t aflags,
                                int64_t aTimeoutUs) {
  // Read Lock for mCodec
  {
    RWLock::AutoRLock autolock(mCodecLock);
    if (mCodec == nullptr) {
      LOGE("MediaCodec has not been inited from input!");
      return NO_INIT;
    }
  }

  size_t index;
  status_t err = dequeueInputBuffer(&index, aTimeoutUs);
  if (err != OK) {
    if (err != -EAGAIN) {
      LOGE("dequeueInputBuffer returned %d", err);
    }
    return err;
  }

  if (aData) {
    const sp<MediaCodecBuffer>& dstBuffer = mInputBuffers.itemAt(index);

    CHECK_LE(aDataSize, dstBuffer->capacity());
    dstBuffer->setRange(0, aDataSize);

    memcpy(dstBuffer->data(), aData, aDataSize);
    err =
        queueInputBuffer(index, 0, dstBuffer->size(), aTimestampUsecs, aflags);
  } else {
    LOG("queueInputBuffer MediaCodec::BUFFER_FLAG_EOS");
    err = queueInputBuffer(index, 0, 0, 0ll, MediaCodec::BUFFER_FLAG_EOS);
  }

  if (err != OK) {
    LOGE("queueInputBuffer returned %d", err);
    return err;
  }
  return err;
}

MediaBuffer* MediaCodecProxy::CreateMediaBuffer(
    const sp<MediaCodecBuffer>& aMediaCodecBuffer) {
  const sp<ABuffer> buffer =
      new ABuffer(aMediaCodecBuffer->data(), aMediaCodecBuffer->capacity());
  buffer->setRange(aMediaCodecBuffer->offset(), aMediaCodecBuffer->size());
  return new MediaBuffer(buffer);
}

MediaBuffer* MediaCodecProxy::CreateMediaBuffer(
    const GraphicBuffer* aGraphicBuffer, size_t aSize) {
  MediaBuffer* buffer = new MediaBuffer(NULL, aSize);
  buffer->meta_data().setPointer(kKeyGraphicBuffer, (void*)aGraphicBuffer);
  return buffer;
}

status_t MediaCodecProxy::Output(MediaBuffer** aBuffer, int64_t aTimeoutUs) {
  // Read Lock for mCodec
  {
    RWLock::AutoRLock autolock(mCodecLock);
    if (mCodec == nullptr) {
      LOGE("MediaCodec has not been inited from output!");
      return NO_INIT;
    }
  }

  size_t index = 0;
  size_t offset = 0;
  size_t size = 0;
  int64_t timeUs = 0;
  uint32_t flags = 0;

  *aBuffer = nullptr;

  status_t err =
      dequeueOutputBuffer(&index, &offset, &size, &timeUs, &flags, aTimeoutUs);
  if (err != OK) {
    LOGE("dequeueOutputBuffer error:%d", err);
    return err;
  }

  MediaBuffer* buffer;
  sp<GraphicBuffer> graphicBuffer;

  if (getOutputGraphicBufferFromIndex(index, &graphicBuffer) == OK &&
      graphicBuffer != nullptr) {
    buffer = CreateMediaBuffer(graphicBuffer.get(), size);
    mOutputGraphicBuffers.insertAt(graphicBuffer, index);
  } else {
    buffer = CreateMediaBuffer(mOutputBuffers.itemAt(index));
  }
  MetaDataBase& metaData = buffer->meta_data();
  metaData.setInt32(kKeyBufferIndex, index);
  metaData.setInt64(kKeyTime, timeUs);
  *aBuffer = buffer;
#ifdef DEBUG_BUFFER_USAGE
  LOG("Output time:%lld, index:%d", timeUs, index);
#endif
  if (flags & MediaCodec::BUFFER_FLAG_EOS) {
    LOG("Got EOS from MediaCodec.");
    return ERROR_END_OF_STREAM;
  }
  return err;
}

void MediaCodecProxy::ReleaseMediaResources() { ReleaseMediaCodec(); }

void MediaCodecProxy::ReleaseMediaBuffer(MediaBuffer* aBuffer) {
  if (aBuffer) {
    MetaDataBase& metaData = aBuffer->meta_data();
    int32_t index;
    metaData.findInt32(kKeyBufferIndex, &index);
#ifdef DEBUG_BUFFER_USAGE
    int64_t timeUs;
    metaData.findInt64(kKeyTime, &timeUs);
    LOG("ReleaseMediaBuffer time:%lld, index:%d", timeUs, index);
#endif
    GraphicBuffer* srcBuffer = nullptr;
    if (aBuffer->meta_data().findPointer(MediaCodecProxy::kKeyGraphicBuffer,
                                         (void**)&srcBuffer)) {
      mOutputGraphicBuffers.removeAt(index);
    }
    aBuffer->release();
    releaseOutputBuffer(index);
  }
}

#ifdef DEBUG_BUFFER_USAGE
void MediaCodecProxy::PrintBufferStats(const char* aFunc, const int aIndex) {
  uint numInputBuffers = 0;
  uint numOutputBuffers = 0;

  for (uint i = 0; i < mInputBuffersCounter.size(); i++) {
    if (mInputBuffersCounter[i] > 0) {
      numInputBuffers++;
    }
  }

  for (uint j = 0; j < mOutputBuffersCounter.size(); j++) {
    if (mOutputBuffersCounter[j] > 0) {
      numOutputBuffers++;
    }
  }

  if (aFunc != NULL && aIndex >= 0) {
    LOG("After %s(%d), Gecko owns MediaCodec %d/%d input buffers and %d/%d "
        "output buffers",
        aFunc, aIndex, numInputBuffers, mInputBuffersCounter.size(),
        numOutputBuffers, mOutputBuffersCounter.size());
  } else if (aFunc != NULL) {
    LOG("After %s, Gecko owns MediaCodec %d/%d input buffers and %d/%d output "
        "buffers",
        aFunc, numInputBuffers, mInputBuffersCounter.size(), numOutputBuffers,
        mOutputBuffersCounter.size());
  } else {
    LOG("Gecko owns MediaCodec %d/%d input buffers and %d/%d output buffers",
        numInputBuffers, mInputBuffersCounter.size(), numOutputBuffers,
        mOutputBuffersCounter.size());
  }
}
#endif

}  // namespace android
