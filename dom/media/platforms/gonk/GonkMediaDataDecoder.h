/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GonkMediaDataDecoder_h_
#define GonkMediaDataDecoder_h_
#include "PlatformDecoderModule.h"
#include "mozilla/CDMProxy.h"
#include "mozilla/UniquePtr.h"
#include <media/stagefright/foundation/AHandler.h>

namespace android {
struct ALooper;
class ICrypto;
class MOZ_EXPORT MediaBuffer;
class MediaCodecProxy;
}  // namespace android

namespace mozilla {
class MediaRawData;

// Callback to communicate with GonkDecoderManager.
class GonkDecoderManagerCallback {
 public:
  // Called by GonkDecoderManager when samples have been decoded.
  virtual void Output(MediaDataDecoder::DecodedData&& aDecodedData) = 0;

  // Denotes that the last input sample has been inserted into the decoder,
  // and no more output can be produced unless more input is sent.
  virtual void InputExhausted() = 0;

  virtual void DrainComplete() = 0;

  virtual void NotifyError(const char* aCallSite, nsresult aResult) = 0;

 protected:
  virtual ~GonkDecoderManagerCallback() {}
};

// Manage the data flow from inputting encoded data and outputting decode data.
class GonkDecoderManager : public android::AHandler {
 public:
  typedef TrackInfo::TrackType TrackType;
  typedef MediaDataDecoder::InitPromise InitPromise;

  virtual ~GonkDecoderManager() {}

  virtual RefPtr<InitPromise> Init() = 0;
  virtual const char* GetDescriptionName() const = 0;
  virtual TrackType GetTrackType() const = 0;

  // Asynchronously send sample into mDecoder. If out of input buffer, aSample
  // will be queued for later re-send.
  nsresult Input(MediaRawData* aSample);

  // Flush the queued samples and signal decoder to throw all pending
  // input/output away.
  nsresult Flush();

  // Shutdown decoder and rejects the init promise.
  nsresult Shutdown();

  // How many samples are waiting for processing.
  size_t NumQueuedSamples();

  // Set callback for decoder events, such as requesting more input,
  // returning output, or reporting error.
  void SetDecodeCallback(GonkDecoderManagerCallback* aCallback) {
    mCallback = aCallback;
  }

 protected:
  GonkDecoderManager(CDMProxy* aProxy) : mCDMProxy(aProxy) {}

  bool InitThreads(MediaData::Type aType);

  void onMessageReceived(
      const android::sp<android::AMessage>& aMessage) override;

  // Produces decoded output. It returns NS_OK on success, or
  // NS_ERROR_NOT_AVAILABLE when output is not produced yet. If this returns a
  // failure code other than NS_ERROR_NOT_AVAILABLE, an error will be reported
  // through mDecodeCallback.
  virtual nsresult GetOutput(int64_t aStreamOffset,
                             MediaDataDecoder::DecodedData& aOutput) = 0;

  // Flush derived class.
  virtual void FlushInternal() = 0;

  // Shutdown derived class.
  virtual void ShutdownInternal() {}

  // Send queued samples to OMX. It returns how many samples are still in
  // queue after processing, or negative error code if failed.
  int32_t ProcessQueuedSamples();

  void ProcessInput();
  void ProcessToDo();

  void AssertOnTaskQueue() { MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn()); }

  android::sp<android::ICrypto> GetCrypto();

  nsAutoCString mMimeType;

  // MediaCodedc's wrapper that performs the decoding.
  android::sp<android::MediaCodecProxy> mDecoder;
  // Looper for mDecoder to run on.
  android::sp<android::ALooper> mDecodeLooper;
  // Looper to run decode tasks such as processing input, output, flush, and
  // recycling output buffers.
  android::sp<android::ALooper> mTaskLooper;
  // Message codes for tasks running on mTaskLooper.
  enum {
    // Decoder will send this to indicate internal state change such as input or
    // output buffers availability. Used to run pending input & output tasks.
    kNotifyDecoderActivity = 'nda ',
  };

  RefPtr<TaskQueue> mTaskQueue;

  MozPromiseHolder<InitPromise> mInitPromise;

  bool mIsShutdown = false;

  // A queue that stores the samples waiting to be sent to mDecoder.
  // Empty element means EOS and there shouldn't be any sample be queued after
  // it. Samples are queued in caller's thread and dequeued in mTaskLooper.
  nsTArray<RefPtr<MediaRawData>> mQueuedSamples;

  bool mInputEOS = false;
  bool mOutputEOS = false;

  // The last decoded frame presentation time. Only accessed on mTaskLooper.
  int64_t mLastTime = INT64_MIN;

  // Remembers the notification that is currently waiting for the decoder event
  // to avoid requesting more than one notification at the time, which is
  // forbidden by mDecoder.
  android::sp<android::AMessage> mToDo;

  // Stores sample info for output buffer processing later.
  struct WaitOutputInfo {
    WaitOutputInfo(int64_t aOffset, int64_t aTimestamp, bool aEOS)
        : mOffset(aOffset), mTimestamp(aTimestamp), mEOS(aEOS) {}
    const int64_t mOffset;
    const int64_t mTimestamp;
    const bool mEOS;
  };

  nsTArray<WaitOutputInfo> mWaitOutput;

  // Reports decoder output or error.
  GonkDecoderManagerCallback* mCallback = nullptr;

  RefPtr<CDMProxy> mCDMProxy;

 private:
  void UpdateWaitingList(int64_t aForgetUpTo);
};

class DecoderManagerCallback;

// Samples are decoded using the GonkDecoder (MediaCodec)
// created by the GonkDecoderManager. This class implements
// the higher-level logic that drives mapping the Gonk to the async
// MediaDataDecoder interface. The specifics of decoding the exact stream
// type are handled by GonkDecoderManager and the GonkDecoder it creates.
class GonkMediaDataDecoder final : public MediaDataDecoder,
                                   public GonkDecoderManagerCallback {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GonkMediaDataDecoder, final);

  GonkMediaDataDecoder(GonkDecoderManager* aDecoderManager);

  RefPtr<InitPromise> Init() override;
  RefPtr<DecodePromise> Decode(MediaRawData* aSample) override;
  RefPtr<FlushPromise> Flush() override;
  RefPtr<DecodePromise> Drain() override;
  RefPtr<ShutdownPromise> Shutdown() override;

  nsCString GetDescriptionName() const override { return "gonk decoder"_ns; }

  nsCString GetCodecName() const override;

  ConversionRequired NeedsConversion() const override {
    return ConversionRequired::kNeedAnnexB;
  }

  // For GonkDecoderManagerCallback interfaces:
  void Output(DecodedData&& aDecodedData) override;
  void InputExhausted() override;
  void DrainComplete() override;
  void NotifyError(const char* aCallSite, nsresult aResult) override;

 private:
  ~GonkMediaDataDecoder();

  void ResolveDecodePromise();
  void ResolveDrainPromise();
  void AssertOnTaskQueue() { MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn()); }

  android::sp<GonkDecoderManager> mManager;

  RefPtr<TaskQueue> mTaskQueue;

  MozPromiseHolder<DecodePromise> mDecodePromise;
  MozPromiseHolder<DecodePromise> mDrainPromise;
  // Where decoded samples will be stored until the decode promise is resolved.
  DecodedData mDecodedData;
};

}  // namespace mozilla

#endif  // GonkMediaDataDecoder_h_
