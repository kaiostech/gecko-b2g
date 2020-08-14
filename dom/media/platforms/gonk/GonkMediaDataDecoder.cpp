/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GonkMediaDataDecoder.h"
#include "VideoUtils.h"
#include "nsTArray.h"
#include "MediaCodecProxy.h"

#include <media/stagefright/foundation/ADebug.h>

#include "mozilla/Logging.h"

mozilla::LazyLogModule gGonkMediaDataDecoderLog("GonkMediaDataDecoder");
mozilla::LazyLogModule gGonkDecoderManagerLog("GonkDecoderManager");
#undef GMDD_LOG
#undef GMDD_LOGE

#define GMDD_LOG(x, ...)                                      \
  MOZ_LOG(gGonkMediaDataDecoderLog, mozilla::LogLevel::Debug, \
          ("%p " x, this, ##__VA_ARGS__))
#define GMDD_LOGE(x, ...)                                     \
  MOZ_LOG(gGonkMediaDataDecoderLog, mozilla::LogLevel::Error, \
          ("%p " x, this, ##__VA_ARGS__))

#undef GDM_LOG
#undef GDM_LOGE

#define GDM_LOG(x, ...)                                     \
  MOZ_LOG(gGonkDecoderManagerLog, mozilla::LogLevel::Debug, \
          ("%p " x, this, ##__VA_ARGS__))
#define GDM_LOGE(x, ...)                                    \
  MOZ_LOG(gGonkDecoderManagerLog, mozilla::LogLevel::Error, \
          ("%p " x, this, ##__VA_ARGS__))

#define INPUT_TIMEOUT_US 0LL  // Don't wait for buffer if none is available.
#define MIN_QUEUED_SAMPLES 2

#ifdef DEBUG
#  include <utils/AndroidThreads.h>
#endif

using namespace android;

namespace mozilla {

bool GonkDecoderManager::InitLoopers(MediaData::Type aType) {
  MOZ_ASSERT(mDecodeLooper.get() == nullptr && mTaskLooper.get() == nullptr);
  MOZ_ASSERT(aType == MediaData::Type::VIDEO_DATA ||
             aType == MediaData::Type::AUDIO_DATA);

  const char* suffix =
      (aType == MediaData::Type::VIDEO_DATA ? "video" : "audio");
  mDecodeLooper = new ALooper;
  android::AString name("MediaCodecProxy/");
  name.append(suffix);
  mDecodeLooper->setName(name.c_str());

  mTaskLooper = new ALooper;
  name.setTo("GonkDecoderManager/");
  name.append(suffix);
  mTaskLooper->setName(name.c_str());
  mTaskLooper->registerHandler(this);

#ifdef DEBUG
  sp<AMessage> findThreadId(new AMessage(kNotifyFindLooperId, this));
  findThreadId->post();
#endif

  return mDecodeLooper->start() == OK && mTaskLooper->start() == OK;
}

nsresult GonkDecoderManager::Input(MediaRawData* aSample) {
  RefPtr<MediaRawData> sample;

  if (aSample) {
    sample = aSample;
  } else {
    // It means EOS with empty sample.
    sample = new MediaRawData();
  }
  {
    MutexAutoLock lock(mMutex);
    mQueuedSamples.AppendElement(sample);
  }
  sp<AMessage> input = new AMessage(kNotifyProcessInput, this);
  if (!aSample) {
    input->setInt32("input-eos", 1);
  }
  input->post();
  return NS_OK;
}

int32_t GonkDecoderManager::ProcessQueuedSamples() {
  MOZ_ASSERT(OnTaskLooper());

  MutexAutoLock lock(mMutex);
  status_t rv;
  while (mQueuedSamples.Length()) {
    RefPtr<MediaRawData> data = mQueuedSamples.ElementAt(0);
    rv = mDecoder->Input(reinterpret_cast<const uint8_t*>(data->Data()),
                         data->Size(), data->mTime.ToMicroseconds(), 0,
                         INPUT_TIMEOUT_US);
    if (rv == OK) {
      mQueuedSamples.RemoveElementAt(0);
      mWaitOutput.AppendElement(
          WaitOutputInfo(data->mOffset, data->mTime.ToMicroseconds(),
                         /* eos */ data->Data() == nullptr));
    } else if (rv == -EAGAIN || rv == -ETIMEDOUT) {
      // In most cases, EAGAIN or ETIMEOUT are safe because OMX can't fill
      // buffer on time.
      break;
    } else {
      return rv;
    }
  }
  return mQueuedSamples.Length();
}

nsresult GonkDecoderManager::Flush() {
  if (mDecoder == nullptr) {
    GDM_LOGE("Decoder is not initialized");
    return NS_ERROR_UNEXPECTED;
  }

  if (!mInitPromise.IsEmpty()) {
    return NS_OK;
  }

  {
    MutexAutoLock lock(mMutex);
    mQueuedSamples.Clear();
  }

  MonitorAutoLock lock(mFlushMonitor);
  mIsFlushing = true;
  sp<AMessage> flush = new AMessage(kNotifyProcessFlush, this);
  flush->post();
  while (mIsFlushing) {
    lock.Wait();
  }
  return NS_OK;
}

nsresult GonkDecoderManager::Shutdown() {
  if (mDecoder.get()) {
    mDecoder->stop();
    mDecoder->ReleaseMediaResources();
    mDecoder = nullptr;
  }

  mInitPromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);

  return NS_OK;
}

size_t GonkDecoderManager::NumQueuedSamples() {
  MutexAutoLock lock(mMutex);
  return mQueuedSamples.Length();
}

void GonkDecoderManager::ProcessInput(bool aEndOfStream) {
  MOZ_ASSERT(OnTaskLooper());

  status_t rv = ProcessQueuedSamples();
  if (rv >= 0) {
    if (!aEndOfStream && rv <= MIN_QUEUED_SAMPLES) {
      mToGonkMediaDataDecoderCallback->InputExhausted();
    }

    if (mToDo.get() == nullptr) {
      mToDo = new AMessage(kNotifyDecoderActivity, this);
      if (aEndOfStream) {
        mToDo->setInt32("input-eos", 1);
      }
      mDecoder->requestActivityNotification(mToDo);
    } else if (aEndOfStream) {
      mToDo->setInt32("input-eos", 1);
    }
  } else {
    GDM_LOGE("input processed: error#%d", rv);
    mToGonkMediaDataDecoderCallback->NotifyError(
        __func__, MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR, __func__));
  }
}

void GonkDecoderManager::ProcessFlush() {
  MOZ_ASSERT(OnTaskLooper());

  mLastTime = INT64_MIN;
  MonitorAutoLock lock(mFlushMonitor);
  mWaitOutput.Clear();
  if (mDecoder->flush() != OK) {
    GDM_LOGE("flush error");
    mToGonkMediaDataDecoderCallback->NotifyError(
        __func__, MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR, __func__));
  }
  mIsFlushing = false;
  lock.NotifyAll();
}

// Use output timestamp to determine which output buffer is already returned
// and remove corresponding info, except for EOS, from the waiting list.
// This method handles the cases that audio decoder sends multiple output
// buffers for one input.
void GonkDecoderManager::UpdateWaitingList(int64_t aForgetUpTo) {
  MOZ_ASSERT(OnTaskLooper());

  size_t i;
  for (i = 0; i < mWaitOutput.Length(); i++) {
    const auto& item = mWaitOutput.ElementAt(i);
    if (item.mEOS || item.mTimestamp > aForgetUpTo) {
      break;
    }
  }
  if (i > 0) {
    mWaitOutput.RemoveElementsAt(0, i);
  }
}

void GonkDecoderManager::ProcessToDo(bool aEndOfStream) {
  MOZ_ASSERT(OnTaskLooper());

  MOZ_ASSERT(mToDo.get() != nullptr);
  mToDo.clear();

  if (NumQueuedSamples() > 0 && ProcessQueuedSamples() < 0) {
    mToGonkMediaDataDecoderCallback->NotifyError(
        __func__, MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR, __func__));
    return;
  }

  while (mWaitOutput.Length() > 0) {
    MediaDataDecoder::DecodedData output;
    WaitOutputInfo wait = mWaitOutput.ElementAt(0);
    nsresult rv = GetOutput(wait.mOffset, output);
    if (rv == NS_OK) {
      UpdateWaitingList(output[output.Length() - 1]->mTime.ToMicroseconds());
      mToGonkMediaDataDecoderCallback->Output(output);
    } else if (rv == NS_ERROR_ABORT) {
      // EOS
      MOZ_ASSERT(mQueuedSamples.IsEmpty());
      if (output.Length() > 0) {
        UpdateWaitingList(output[output.Length() - 1]->mTime.ToMicroseconds());
        mToGonkMediaDataDecoderCallback->Output(output);
      }
      MOZ_ASSERT(mWaitOutput.Length() == 1);
      mWaitOutput.RemoveElementAt(0);
      mToGonkMediaDataDecoderCallback->DrainComplete();
      ResetEOS();
      return;
    } else if (rv == NS_ERROR_NOT_AVAILABLE) {
      break;
    } else {
      mToGonkMediaDataDecoderCallback->NotifyError(
          __func__, MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR, __func__));
      return;
    }
  }

  if (!aEndOfStream && NumQueuedSamples() <= MIN_QUEUED_SAMPLES) {
    mToGonkMediaDataDecoderCallback->InputExhausted();
    // No need to shedule todo task this time because InputExhausted() will
    // cause Input() to be invoked and do it for us.
    return;
  }

  if (NumQueuedSamples() || mWaitOutput.Length() > 0) {
    mToDo = new AMessage(kNotifyDecoderActivity, this);
    if (aEndOfStream) {
      mToDo->setInt32("input-eos", 1);
    }
    mDecoder->requestActivityNotification(mToDo);
  }
}

void GonkDecoderManager::ResetEOS() {
  // After eos, android::MediaCodec needs to be flushed to receive next input
  mWaitOutput.Clear();
  if (mDecoder->flush() != OK) {
    GDM_LOGE("flush error");
    mToGonkMediaDataDecoderCallback->NotifyError(
        __func__, MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR, __func__));
  }
}

void GonkDecoderManager::onMessageReceived(const sp<AMessage>& aMessage) {
  switch (aMessage->what()) {
    case kNotifyProcessInput: {
      int32_t eos = 0;
      ProcessInput(aMessage->findInt32("input-eos", &eos) && eos);
      break;
    }
    case kNotifyProcessFlush: {
      ProcessFlush();
      break;
    }
    case kNotifyDecoderActivity: {
      int32_t eos = 0;
      ProcessToDo(aMessage->findInt32("input-eos", &eos) && eos);
      break;
    }
#ifdef DEBUG
    case kNotifyFindLooperId: {
      mTaskLooperId = androidGetThreadId();
      MOZ_ASSERT(mTaskLooperId);
      break;
    }
#endif
    default: {
      TRESPASS();
      break;
    }
  }
}

#ifdef DEBUG
bool GonkDecoderManager::OnTaskLooper() {
  return androidGetThreadId() == mTaskLooperId;
}
#endif

AutoReleaseMediaBuffer::AutoReleaseMediaBuffer(android::MediaBuffer* aBuffer,
                                               android::MediaCodecProxy* aCodec)
    : mBuffer(aBuffer), mCodec(aCodec) {}

AutoReleaseMediaBuffer::~AutoReleaseMediaBuffer() {
  MOZ_ASSERT(mCodec.get());
  if (mBuffer) {
    mCodec->ReleaseMediaBuffer(mBuffer);
  }
}

android::MediaBuffer* AutoReleaseMediaBuffer::forget() {
  android::MediaBuffer* tmp = mBuffer;
  mBuffer = nullptr;
  return tmp;
}

GonkMediaDataDecoder::GonkMediaDataDecoder(GonkDecoderManager* aManager,
                                           TaskQueue* aTaskQueue)
    : mManager(aManager), mTaskQueue(aTaskQueue) {
  MOZ_COUNT_CTOR(GonkMediaDataDecoder);
  mCallback = new DecoderManagerCallback(this);
  mManager->SetDecodeCallback(mCallback);
}

GonkMediaDataDecoder::~GonkMediaDataDecoder() {
  MOZ_COUNT_DTOR(GonkMediaDataDecoder);
}

RefPtr<MediaDataDecoder::InitPromise> GonkMediaDataDecoder::Init() {
  return mManager->Init();
}

RefPtr<ShutdownPromise> GonkMediaDataDecoder::Shutdown() {
  RefPtr<MediaDataDecoder> self = this;
  return InvokeAsync(mTaskQueue, __func__, [self, this]() {
    RefPtr<ShutdownPromise> p = mShutdownPromise.Ensure(__func__);

    mInitPromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
    nsresult rv = mManager->Shutdown();
    // Because codec allocated runnable and init promise is at reader TaskQueue,
    // so manager needs to be destroyed at reader TaskQueue to prevent racing.
    mManager = nullptr;

    if (rv == NS_OK) {
      mShutdownPromise.ResolveIfExists(true, __func__);
    } else {
      mShutdownPromise.RejectIfExists(false, __func__);
    }
    return p;
  });
}

// Inserts data into the decoder's pipeline.
RefPtr<MediaDataDecoder::DecodePromise> GonkMediaDataDecoder::Decode(
    MediaRawData* aSample) {
  RefPtr<DecodePromise> p = mDecodePromise.Ensure(__func__);
  mManager->Input(aSample);
  return p;
}

RefPtr<MediaDataDecoder::FlushPromise> GonkMediaDataDecoder::Flush() {
  RefPtr<MediaDataDecoder> self = this;
  return InvokeAsync(mTaskQueue, __func__, [self, this]() {
    RefPtr<FlushPromise> p = mFlushPromise.Ensure(__func__);

    if (mManager->Flush() == NS_OK) {
      mFlushPromise.ResolveIfExists(true, __func__);
    } else {
      mFlushPromise.RejectIfExists(NS_ERROR_DOM_MEDIA_FATAL_ERR, __func__);
    }
    return p;
  });
}

RefPtr<MediaDataDecoder::DecodePromise> GonkMediaDataDecoder::Drain() {
  RefPtr<DecodePromise> p = mDrainPromise.Ensure(__func__);
  // Send nullpter to GonkDecoderManager will trigger the EOS procedure.
  mManager->Input(nullptr);
  return p;
}

void GonkMediaDataDecoder::Output(DecodedData& aDataArray) {
  for (RefPtr<MediaData> data : aDataArray) {
    mDecodedData.AppendElement(std::move(data));
  }
  ResolveDecodePromise();
}

void GonkMediaDataDecoder::InputExhausted() { ResolveDecodePromise(); }

void GonkMediaDataDecoder::DrainComplete() { ResolveDecodePromise(); }

void GonkMediaDataDecoder::ReleaseMediaResources() {}

void GonkMediaDataDecoder::NotifyError(const char* aLine,
                                       const MediaResult& aError) {
  GMDD_LOGE("NotifyError (%s) at %s", aError.ErrorName().get(), aLine);
  mDecodedData = DecodedData();
  mDecodePromise.RejectIfExists(aError, __func__);
  mDrainPromise.RejectIfExists(aError, __func__);
  mFlushPromise.RejectIfExists(aError, __func__);
}

void GonkMediaDataDecoder::ResolveDecodePromise() {
  if (mDecodePromise.IsEmpty() == false) {
    mDecodePromise.Resolve(std::move(mDecodedData), __func__);
    mDecodedData = DecodedData();
  }
}

}  // namespace mozilla
