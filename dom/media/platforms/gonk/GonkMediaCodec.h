/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GONK_MEDIA_CODEC_H
#define GONK_MEDIA_CODEC_H

#include <media/stagefright/foundation/AHandler.h>
#include <media/stagefright/MediaCodec.h>
#include <utils/RefBase.h>

#include "mozilla/media/MediaSystemResourceClient.h"

namespace mozilla {
namespace layers {
class TextureClient;
}
}  // namespace mozilla

namespace android {

class GonkCryptoInfo;

class GonkMediaCodec final
    : public AHandler,
      public mozilla::MediaSystemResourceReservationListener {
  using TextureClient = mozilla::layers::TextureClient;

 public:
  // A Reply carries the result of a GonkMediaCodec operation. The client can
  // acquire the result from it either synchronously or asynchronously. A Reply
  // returned from GonkMediaCodec should always be handled by the client through
  // Then(), Wait() or Ignore(). Otherwise it will assert.
  class Reply : public RefBase {
   public:
    // Like a promise, specify an async callable that will be invoked later with
    // the result. The callable is always invoked on GonkMediaCodec's looper
    // thread.
    using Callable = std::function<void(status_t)>;
    virtual void Then(const Callable& aCallable) = 0;

    // Get the result using blocking wait.
    virtual status_t Wait() = 0;

    // Deliberately ignore the result and don't wait.
    virtual void Ignore() = 0;
  };

  // All data and codec events are communicated through a Callback object. Each
  // function is always called on GonkMediaCodec's looper thread.
  class Callback : public RefBase {
   public:
    virtual bool FetchInput(const sp<MediaCodecBuffer>& aBuffer,
                            sp<RefBase>* aInputInfo,
                            sp<GonkCryptoInfo>* aCryptoInfo, int64_t* aTimeUs,
                            uint32_t* aFlags) = 0;

    virtual void Output(const sp<MediaCodecBuffer>& aBuffer,
                        const sp<RefBase>& aInputInfo, int64_t aTimeUs,
                        uint32_t aFlags) = 0;

    virtual void OutputTexture(TextureClient* aTexture,
                               const sp<RefBase>& aInputInfo, int64_t aTimeUs,
                               uint32_t aFlags) {}

    virtual void NotifyOutputFormat(const sp<AMessage>& aFormat) {}

    virtual void NotifyCodecDetails(const sp<AMessage>& aDetails) {}

    virtual void NotifyError(status_t aErr, int32_t aActionCode) = 0;
  };

  // Provide a default implementation of callback proxy.
  template <class Owner>
  class CallbackProxy;

  GonkMediaCodec();

  void Init();

  [[nodiscard]] sp<Reply> Shutdown();

  [[nodiscard]] sp<Reply> Flush();

  [[nodiscard]] sp<Reply> Configure(const sp<Callback>& aCallback,
                                    const sp<AMessage>& aFormat,
                                    const sp<ICrypto>& aCrypto, bool aEncoder);

  void InputUpdated();

 private:
  class CodecNativeWindow;
  class InputInfoQueue;
  class ReplyImpl;

  enum {
    kWhatCodecNotify = 'codc',
    kWhatConfigure = 'conf',
    kWhatResourceReserved = 'resR',
    kWhatResourceReservationFailed = 'resF',
    kWhatShutdown = 'shuD',
    kWhatFlush = 'flus',
    kWhatInputUpdated = 'inUp',
    kWhatReleaseOutput = 'relO',
    kWhatNotifyOutput = 'notO',
    kWhatPrintBufferQueueStats = 'prBQ',
  };

  virtual ~GonkMediaCodec();

  void onMessageReceived(const sp<AMessage>& aMsg) override;

  status_t OnConfigure(const sp<Callback>& aCallback,
                       const sp<AMessage>& aFormat, const sp<ICrypto>& aCrypto,
                       bool aEncoder);

  status_t OnShutdown();

  status_t OnFlush();

  void OnInputUpdated();

  void OnOutputAvailable(int32_t aIndex, size_t aOffset, size_t aSize,
                         int64_t aTimeUs, int32_t aFlags);

  void OnReleaseOutput();

  void OnNotifyOutput(const sp<AMessage>& aMsg);

  sp<Surface> InitBufferQueue();

  sp<ALooper> mLooper;
  sp<ALooper> mCodecLooper;
  sp<MediaCodec> mCodec;
  sp<Callback> mCallback;
  sp<AMessage> mConfigMsg;
  sp<CodecNativeWindow> mNativeWindow;
  std::list<size_t> mInputBuffers;
  std::list<sp<AMessage>> mOutputBuffers;
  std::unique_ptr<InputInfoQueue> mInputInfoQueue;

  // Media resource management.
  void ReserveResource(const sp<AMessage>& aFormat, bool aEncoder);
  void ResourceReserved() override;
  void ResourceReserveFailed() override;
  RefPtr<mozilla::MediaSystemResourceClient> mResourceClient;
};

// SFINAE templates that detect the existence of a member function.
#define HAS_MEMBER_FUNC(func)             \
  template <class T, class = void>        \
  struct Has_##func : std::false_type {}; \
                                          \
  template <class T>                      \
  struct Has_##func<T, std::void_t<decltype(&T::func)>> : std::true_type {};

template <class Owner>
class GonkMediaCodec::CallbackProxy final : public Callback {
  // These callback functions can be optionally implemented by the owner. Define
  // SFINAE templates for them here so we can check their existence at compile
  // time.
  HAS_MEMBER_FUNC(OutputTexture)
  HAS_MEMBER_FUNC(NotifyOutputFormat)
  HAS_MEMBER_FUNC(NotifyCodecDetails)

 public:
  static sp<CallbackProxy> Create(Owner* aOwner) {
    return new CallbackProxy(aOwner);
  }

  bool FetchInput(const sp<MediaCodecBuffer>& aBuffer, sp<RefBase>* aInputInfo,
                  sp<GonkCryptoInfo>* aCryptoInfo, int64_t* aTimeUs,
                  uint32_t* aFlags) override {
    return mOwner->FetchInput(aBuffer, aInputInfo, aCryptoInfo, aTimeUs,
                              aFlags);
  }

  void Output(const sp<MediaCodecBuffer>& aBuffer,
              const sp<RefBase>& aInputInfo, int64_t aTimeUs,
              uint32_t aFlags) override {
    mOwner->Output(aBuffer, aInputInfo, aTimeUs, aFlags);
  }

  void OutputTexture(TextureClient* aTexture, const sp<RefBase>& aInputInfo,
                     int64_t aTimeUs, uint32_t aFlags) override {
    if constexpr (Has_OutputTexture<Owner>::value) {
      mOwner->OutputTexture(aTexture, aInputInfo, aTimeUs, aFlags);
    } else {
      TRESPASS("OutputTexture() not implemented");
    }
  }

  void NotifyOutputFormat(const sp<AMessage>& aFormat) override {
    if constexpr (Has_NotifyOutputFormat<Owner>::value) {
      mOwner->NotifyOutputFormat(aFormat);
    }
  }

  void NotifyCodecDetails(const sp<AMessage>& aDetails) override {
    if constexpr (Has_NotifyCodecDetails<Owner>::value) {
      mOwner->NotifyCodecDetails(aDetails);
    }
  }

  void NotifyError(status_t aErr, int32_t aActionCode) override {
    mOwner->NotifyError(aErr, aActionCode);
  }

 private:
  CallbackProxy(Owner* aOwner) : mOwner(aOwner) {}

  Owner* mOwner = nullptr;
};

#undef HAS_MEMBER_FUNC

}  // namespace android

#endif  // GONK_MEDIA_CODEC_H
