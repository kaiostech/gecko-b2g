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
#include <media/stagefright/foundation/AMessage.h>
#include <mediadrm/ICrypto.h>

#include "mozilla/Logging.h"

#ifdef B2G_MEDIADRM
#  include "mozilla/GonkDrmCDMProxy.h"
#endif

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

bool GonkDecoderManager::InitThreads(MediaData::Type aType) {
  MOZ_ASSERT(!mTaskQueue && !mDecodeLooper && !mTaskLooper);
  MOZ_ASSERT(aType == MediaData::Type::VIDEO_DATA ||
             aType == MediaData::Type::AUDIO_DATA);

  // Use the TaskQueue from GonkMediaDataDecoder.
  mTaskQueue = static_cast<TaskQueue*>(AbstractThread::GetCurrent());
  MOZ_ASSERT(mTaskQueue);

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

  return mDecodeLooper->start() == OK && mTaskLooper->start() == OK;
}

nsresult GonkDecoderManager::Input(MediaRawData* aSample) {
  AssertOnTaskQueue();

  // Multiple drain commands are possible. If we received EOS before, just don't
  // append the new sample, and treat non-null sample as an error. If mOutputEOS
  // is true, ProcessToDo() won't resolve drain promise again, so we need to do
  // it here.
  if (mInputEOS) {
    if (mOutputEOS) {
      mCallback->DrainComplete();
    } else {
      ProcessInput();
    }
    return aSample ? NS_ERROR_DOM_MEDIA_END_OF_STREAM : NS_OK;
  }

  RefPtr<MediaRawData> sample(aSample);
  if (!aSample) {
    // Append empty sample for EOS case.
    sample = new MediaRawData();
    mInputEOS = true;
  }

  mQueuedSamples.AppendElement(sample);
  ProcessInput();
  return NS_OK;
}

#ifdef B2G_MEDIADRM
static void GetCryptoInfoFromSample(const MediaRawData* aSample,
                                    GonkCryptoInfo* aCryptoInfo) {
  auto& cryptoObj = aSample->mCrypto;
  if (!cryptoObj.IsEncrypted()) {
    return;
  }

  // |mPlainSizes| and |mEncryptedSizes| should have the same length. If not,
  // just pad the shorter one with zero.
  auto numSubSamples = std::max(cryptoObj.mPlainSizes.Length(),
                                cryptoObj.mEncryptedSizes.Length());
  auto& subSamples = aCryptoInfo->mSubSamples;
  uint32_t totalSubSamplesSize = 0;
  for (size_t i = 0; i < numSubSamples; i++) {
    auto plainSize = cryptoObj.mPlainSizes.SafeElementAt(i, 0);
    auto encryptedSize = cryptoObj.mEncryptedSizes.SafeElementAt(i, 0);
    totalSubSamplesSize += plainSize + encryptedSize;
    subSamples.push_back({plainSize, encryptedSize});
  }

  // Size of codec specific data("CSD") for MediaCodec usage should be included
  // in the 1st plain size if it exists.
  if (!subSamples.empty()) {
    uint32_t codecSpecificDataSize = aSample->Size() - totalSubSamplesSize;
    // This shouldn't overflow as the the plain size should be UINT16_MAX at
    // most, and the CSD should never be that large. Checked int acts like a
    // diagnostic assert here to help catch if we ever have insane inputs.
    CheckedUint32 newLeadingPlainSize{subSamples[0].mNumBytesOfClearData};
    newLeadingPlainSize += codecSpecificDataSize;
    subSamples[0].mNumBytesOfClearData = newLeadingPlainSize.value();
  }

  static const int kExpectedIVLength = 16;
  aCryptoInfo->mIV.assign(cryptoObj.mIV.begin(), cryptoObj.mIV.end());
  aCryptoInfo->mIV.resize(kExpectedIVLength, 0);  // padding with 0
  static const int kExpectedKeyLength = 16;
  aCryptoInfo->mKey.assign(cryptoObj.mKeyId.begin(), cryptoObj.mKeyId.end());
  aCryptoInfo->mKey.resize(kExpectedKeyLength, 0);  // padding with 0

  aCryptoInfo->mPattern.mEncryptBlocks = cryptoObj.mCryptByteBlock;
  aCryptoInfo->mPattern.mSkipBlocks = cryptoObj.mSkipByteBlock;

  switch (cryptoObj.mCryptoScheme) {
    case CryptoScheme::None:
      aCryptoInfo->mMode = CryptoPlugin::kMode_Unencrypted;
      break;
    case CryptoScheme::Cenc:
      aCryptoInfo->mMode = CryptoPlugin::kMode_AES_CTR;
      break;
    case CryptoScheme::Cbcs:
      aCryptoInfo->mMode = CryptoPlugin::kMode_AES_CBC;
      break;
  }
}
#endif

int32_t GonkDecoderManager::ProcessQueuedSamples() {
  AssertOnTaskQueue();

  status_t rv;
  while (mQueuedSamples.Length()) {
    RefPtr<MediaRawData> data = mQueuedSamples.ElementAt(0);
    GonkCryptoInfo cryptoInfo;
#ifdef B2G_MEDIADRM
    GetCryptoInfoFromSample(data, &cryptoInfo);
#endif
    rv = mDecoder->Input(reinterpret_cast<const uint8_t*>(data->Data()),
                         data->Size(), data->mTime.ToMicroseconds(), 0,
                         INPUT_TIMEOUT_US, &cryptoInfo);
    if (rv == OK) {
      mQueuedSamples.RemoveElementAt(0);
      mWaitOutput.AppendElement(WaitOutputInfo(data->mOffset,
                                               data->mTime.ToMicroseconds(),
                                               /* eos */ !data->Data()));
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
  AssertOnTaskQueue();

  if (!mDecoder) {
    GDM_LOGE("Decoder is not initialized");
    return NS_ERROR_UNEXPECTED;
  }

  if (!mInitPromise.IsEmpty()) {
    return NS_OK;
  }

  FlushInternal();
  mInputEOS = false;
  mOutputEOS = false;
  mLastTime = INT64_MIN;
  mWaitOutput.Clear();
  mQueuedSamples.Clear();

  if (mDecoder->flush() != OK) {
    GDM_LOGE("flush error");
    nsresult rv = NS_ERROR_DOM_MEDIA_DECODE_ERR;
    mCallback->NotifyError(__func__, rv);
    return rv;
  }
  return NS_OK;
}

nsresult GonkDecoderManager::Shutdown() {
  AssertOnTaskQueue();

  mIsShutdown = true;

  if (mDecoder) {
    mDecoder->stop();
    mDecoder->ReleaseMediaResources();
    mDecoder = nullptr;
  }

  if (mDecodeLooper) {
    mDecodeLooper->stop();
    mDecodeLooper = nullptr;
  }

  if (mTaskLooper) {
    mTaskLooper->unregisterHandler(id());
    mTaskLooper->stop();
    mTaskLooper = nullptr;
  }

  // Don't set mTaskQueue to null since other threads may still dispatch tasks
  // to it. Note that after GonkMediaDataDecoder calls
  // mTaskQueue->BeginShutdown(), any Dispatch() call will simply return error.

  mCallback = nullptr;
  mCDMProxy = nullptr;
  ShutdownInternal();
  mInitPromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
  return NS_OK;
}

size_t GonkDecoderManager::NumQueuedSamples() {
  AssertOnTaskQueue();
  return mQueuedSamples.Length();
}

void GonkDecoderManager::ProcessInput() {
  AssertOnTaskQueue();

  status_t rv = ProcessQueuedSamples();
  if (rv >= 0) {
    if (!mInputEOS && rv <= MIN_QUEUED_SAMPLES) {
      mCallback->InputExhausted();
    }

    if (!mToDo) {
      mToDo = new AMessage(kNotifyDecoderActivity, this);
      mDecoder->requestActivityNotification(mToDo);
    }
  } else {
    GDM_LOGE("input processed: error#%d", rv);
    mCallback->NotifyError(__func__, NS_ERROR_DOM_MEDIA_DECODE_ERR);
  }
}

// Use output timestamp to determine which output buffer is already returned
// and remove corresponding info, except for EOS, from the waiting list.
// This method handles the cases that audio decoder sends multiple output
// buffers for one input.
void GonkDecoderManager::UpdateWaitingList(int64_t aForgetUpTo) {
  AssertOnTaskQueue();

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

void GonkDecoderManager::ProcessToDo() {
  AssertOnTaskQueue();

  MOZ_ASSERT(mToDo);
  mToDo = nullptr;

  if (NumQueuedSamples() > 0 && ProcessQueuedSamples() < 0) {
    mCallback->NotifyError(__func__, NS_ERROR_DOM_MEDIA_DECODE_ERR);
    return;
  }

  while (mWaitOutput.Length() > 0) {
    MediaDataDecoder::DecodedData output;
    WaitOutputInfo wait = mWaitOutput.ElementAt(0);
    nsresult rv = GetOutput(wait.mOffset, output);
    if (rv == NS_OK) {
      UpdateWaitingList(output[output.Length() - 1]->mTime.ToMicroseconds());
      mCallback->Output(std::move(output));
    } else if (rv == NS_ERROR_DOM_MEDIA_END_OF_STREAM) {
      // EOS
      MOZ_ASSERT(mQueuedSamples.IsEmpty());
      if (output.Length() > 0) {
        UpdateWaitingList(output[output.Length() - 1]->mTime.ToMicroseconds());
        mCallback->Output(std::move(output));
      }
      MOZ_ASSERT(mWaitOutput.Length() == 1);
      mWaitOutput.RemoveElementAt(0);
      mCallback->DrainComplete();
      mOutputEOS = true;
      return;
    } else if (rv == NS_ERROR_NOT_AVAILABLE) {
      break;
    } else {
      mCallback->NotifyError(__func__, NS_ERROR_DOM_MEDIA_DECODE_ERR);
      return;
    }
  }

  if (!mInputEOS && NumQueuedSamples() <= MIN_QUEUED_SAMPLES) {
    mCallback->InputExhausted();
    // No need to shedule todo task this time because InputExhausted() will
    // cause Input() to be invoked and do it for us.
    return;
  }

  if (NumQueuedSamples() || mWaitOutput.Length() > 0) {
    mToDo = new AMessage(kNotifyDecoderActivity, this);
    mDecoder->requestActivityNotification(mToDo);
  }
}

void GonkDecoderManager::onMessageReceived(const sp<AMessage>& aMessage) {
  switch (aMessage->what()) {
    case kNotifyDecoderActivity: {
      sp<GonkDecoderManager> self = this;
      nsresult rv = mTaskQueue->Dispatch(NS_NewRunnableFunction(
          "GonkDecoderManager::ProcessToDo", [self, this]() {
            // This task may be run after Shutdown() is called. Don't do
            // anything in this case.
            if (!mIsShutdown) {
              ProcessToDo();
            }
          }));
      MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
      Unused << rv;
      break;
    }
    default: {
      TRESPASS();
      break;
    }
  }
}

sp<ICrypto> GonkDecoderManager::GetCrypto() {
#ifdef B2G_MEDIADRM
  if (mCDMProxy) {
    return static_cast<GonkDrmCDMProxy*>(mCDMProxy.get())->CreateCrypto();
  }
#endif
  return nullptr;
}

GonkMediaDataDecoder::GonkMediaDataDecoder(GonkDecoderManager* aManager)
    : mManager(aManager),
      mTaskQueue(
          CreateMediaDecodeTaskQueue("GonkMediaDataDecoder::mTaskQueue")) {
  MOZ_COUNT_CTOR(GonkMediaDataDecoder);
  mManager->SetDecodeCallback(this);
}

GonkMediaDataDecoder::~GonkMediaDataDecoder() {
  MOZ_COUNT_DTOR(GonkMediaDataDecoder);
}

RefPtr<MediaDataDecoder::InitPromise> GonkMediaDataDecoder::Init() {
  RefPtr<MediaDataDecoder> self = this;
  return InvokeAsync(mTaskQueue, __func__,
                     [self, this]() { return mManager->Init(); });
}

RefPtr<ShutdownPromise> GonkMediaDataDecoder::Shutdown() {
  RefPtr<MediaDataDecoder> self = this;
  return InvokeAsync(mTaskQueue, __func__, [self, this]() {
    mManager->Shutdown();
    // Because codec allocated runnable and init promise is at our TaskQueue, so
    // manager needs to be destroyed at our TaskQueue to prevent racing.
    mManager = nullptr;
    return mTaskQueue->BeginShutdown();
  });
}

// Inserts data into the decoder's pipeline.
RefPtr<MediaDataDecoder::DecodePromise> GonkMediaDataDecoder::Decode(
    MediaRawData* aSample) {
  RefPtr<MediaDataDecoder> self = this;
  RefPtr<MediaRawData> sample = aSample;
  return InvokeAsync(mTaskQueue, __func__, [self, this, sample]() {
    RefPtr<DecodePromise> p = mDecodePromise.Ensure(__func__);
    mManager->Input(sample);
    return p;
  });
}

RefPtr<MediaDataDecoder::FlushPromise> GonkMediaDataDecoder::Flush() {
  RefPtr<MediaDataDecoder> self = this;
  return InvokeAsync(mTaskQueue, __func__, [self, this]() {
    // After mManager is flushed, make sure our decoded data is also cleared..
    nsresult rv = mManager->Flush();
    mDecodedData = DecodedData();
    if (NS_FAILED(rv)) {
      return FlushPromise::CreateAndReject(rv, __func__);
    }
    return FlushPromise::CreateAndResolve(true, __func__);
  });
}

RefPtr<MediaDataDecoder::DecodePromise> GonkMediaDataDecoder::Drain() {
  RefPtr<MediaDataDecoder> self = this;
  return InvokeAsync(mTaskQueue, __func__, [self, this]() {
    RefPtr<DecodePromise> p = mDrainPromise.Ensure(__func__);
    // Send nullpter to GonkDecoderManager will trigger the EOS procedure.
    mManager->Input(nullptr);
    return p;
  });
}

void GonkMediaDataDecoder::Output(DecodedData&& aDecodedData) {
  AssertOnTaskQueue();
  mDecodedData.AppendElements(std::move(aDecodedData));
  ResolveDecodePromise();
}

void GonkMediaDataDecoder::InputExhausted() {
  AssertOnTaskQueue();
  ResolveDecodePromise();
}

void GonkMediaDataDecoder::DrainComplete() {
  AssertOnTaskQueue();
  ResolveDecodePromise();
  ResolveDrainPromise();
}

void GonkMediaDataDecoder::NotifyError(const char* aCallSite,
                                       nsresult aResult) {
  AssertOnTaskQueue();
  MediaResult result(aResult, aCallSite);
  GMDD_LOGE("NotifyError (%s) at %s", result.ErrorName().get(), aCallSite);
  mDecodedData = DecodedData();
  mDecodePromise.RejectIfExists(MediaResult(result), __func__);
  mDrainPromise.RejectIfExists(MediaResult(result), __func__);
}

void GonkMediaDataDecoder::ResolveDecodePromise() {
  AssertOnTaskQueue();
  if (!mDecodePromise.IsEmpty()) {
    mDecodePromise.Resolve(std::move(mDecodedData), __func__);
    mDecodedData = DecodedData();
  }
}

void GonkMediaDataDecoder::ResolveDrainPromise() {
  AssertOnTaskQueue();
  if (!mDrainPromise.IsEmpty()) {
    mDrainPromise.Resolve(std::move(mDecodedData), __func__);
    mDecodedData = DecodedData();
  }
}

nsCString GonkMediaDataDecoder::GetCodecName() const {
  return "unknown"_ns;
}

}  // namespace mozilla
