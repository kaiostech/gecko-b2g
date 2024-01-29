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
  class Reply : public RefBase {
   public:
    virtual void Invoke(status_t aErr) = 0;
  };

  class Callback : public RefBase {
   public:
    virtual bool FetchInput(const sp<MediaCodecBuffer>& aBuffer,
                            sp<RefBase>* aInputInfo,
                            sp<GonkCryptoInfo>* aCryptoInfo, int64_t* aTimeUs,
                            uint32_t* aFlags) = 0;

    virtual void Output(const sp<MediaCodecBuffer>& aBuffer,
                        const sp<RefBase>& aInputInfo, int64_t aTimeUs) = 0;

    virtual void Output(TextureClient* aBuffer, const sp<RefBase>& aInputInfo,
                        int64_t aTimeUs) = 0;

    virtual void NotifyOutputEnded() = 0;

    virtual void NotifyOutputFormat(const sp<AMessage>& aFormat) = 0;

    virtual void NotifyCodecDetails(const sp<AMessage>& aDetails) = 0;

    virtual void NotifyError(status_t aErr, int32_t aActionCode) = 0;
  };

  GonkMediaCodec();

  void Init();

  void Shutdown(const sp<Reply>& aReply);

  void Flush(const sp<Reply>& aReply);

  void Configure(const sp<Reply>& aReply, const sp<Callback>& aCallback,
                 const sp<AMessage>& aFormat, const sp<ICrypto>& aCrypto,
                 bool aEncoder);

  void InputUpdated();

 private:
  class CodecNativeWindow;
  class InputInfoQueue;

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

}  // namespace android

#endif  // GONK_MEDIA_CODEC_H
