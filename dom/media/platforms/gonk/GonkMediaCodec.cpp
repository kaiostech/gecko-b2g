/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GonkMediaCodec.h"

#include <binder/ProcessState.h>
#include <gui/Surface.h>
#include <media/MediaCodecBuffer.h>
#include <media/stagefright/foundation/ABuffer.h>
#include <media/stagefright/foundation/AMessage.h>
#include <mediadrm/ICrypto.h>

#include "GonkBufferItem.h"
#include "GonkBufferQueueProducer.h"
#include "GonkMediaUtils.h"
#include "GonkNativeWindow.h"
#include "mozilla/StaticPrefs_media.h"

namespace android {

#undef LOGE
#undef LOGW
#undef LOGI
#undef LOGD
#undef LOGV

static mozilla::LazyLogModule sCodecLog("GonkMediaCodec");
#define LOGE(...) MOZ_LOG(sCodecLog, mozilla::LogLevel::Error, (__VA_ARGS__))
#define LOGW(...) MOZ_LOG(sCodecLog, mozilla::LogLevel::Warning, (__VA_ARGS__))
#define LOGI(...) MOZ_LOG(sCodecLog, mozilla::LogLevel::Info, (__VA_ARGS__))
#define LOGD(...) MOZ_LOG(sCodecLog, mozilla::LogLevel::Debug, (__VA_ARGS__))
#define LOGV(...) MOZ_LOG(sCodecLog, mozilla::LogLevel::Verbose, (__VA_ARGS__))

static inline bool IsVerboseLoggingEnabled() {
  const mozilla::LogModule* logModule = sCodecLog;
  return MOZ_LOG_TEST(logModule, mozilla::LogLevel::Verbose);
}

static std::vector<std::string> SplitMultilineString(const std::string& aStr) {
  std::vector<std::string> lines;
  std::string::size_type begin = 0, end;
  while ((end = aStr.find('\n', begin)) != std::string::npos) {
    lines.push_back(aStr.substr(begin, end - begin));
    begin = end + 1;
  }
  lines.push_back(aStr.substr(begin));
  return lines;
}

// CryptoHolder is needed because we can't downcast RefBase to ICrypto due to
// virtual inheritance.
using CryptoHolder = GonkObjectHolder<sp<ICrypto>>;
using TextureHolder = GonkObjectHolder<RefPtr<mozilla::layers::TextureClient>>;

class GonkMediaCodec::CodecNativeWindow final : public GonkNativeWindow {
  using TextureClient = mozilla::layers::TextureClient;

 public:
  CodecNativeWindow(const sp<IGonkGraphicBufferConsumer>& aConsumer,
                    const sp<AMessage>& aReleaseOutput, int aMaxAcquiredCount)
      : GonkNativeWindow(aConsumer, aMaxAcquiredCount),
        mReleaseOutput(aReleaseOutput),
        mMaxAcquiredCount(aMaxAcquiredCount) {}

  void QueueOutputMessage(const sp<AMessage>& aMsg) {
    CHECK(aMsg->contains("flags") || aMsg->contains("format"));
    Mutex::Autolock lock(mMutex);
    mOutputMessages.push_back(aMsg);
    ProcessOutputMessages(nullptr);
  }

  // Called when flushing. We can't just clear the message queue because any
  // pending texture in the BufferQueue is 1-to-1 mapped to an message with
  // inputInfo in the message queue.
  void CancelPendingOutputMessages() {
    Mutex::Autolock lock(mMutex);
    for (auto& msg : mOutputMessages) {
      msg->setInt32("canceled", true);
    }
  }

  bool CanAcquire() {
    Mutex::Autolock lock(mMutex);
    // Although the max acquired buffer count can be exceeded by 1, that extra
    // buffer can only be acquired briefly. Otherwise dequeueBuffer() at
    // producer side may block until a free slot is available. See the comment
    // in GonkBufferQueueProducer::waitForFreeSlotThenRelock().
    return mConsumer->getAcquiredBufferCount() + GetBufferMessageCount() <
           mMaxAcquiredCount;
  }

  void Release() {
    {
      Mutex::Autolock lock(mMutex);
      mOutputMessages.clear();
    }
    abandon();
  }

  void PrintStats() {
    int queuedCount, acquiredCount;
    std::list<uint64_t> acquiredTextures;
    {
      Mutex::Autolock lock(mMutex);
      queuedCount = GetBufferMessageCount();
      acquiredCount = mConsumer->getAcquiredBufferCount();
      acquiredTextures = mAcquiredTextures;
    }
    auto str =
        AStringPrintf("Texture queued (%d), acquired (%d/%d):", queuedCount,
                      acquiredCount, mMaxAcquiredCount + 1);
    for (auto texture : acquiredTextures) {
      str.append(AStringPrintf(" #%" PRIu64, texture));
    }
    LOGI("%s", str.c_str());
  }

 private:
  void returnBuffer(TextureClient* aBuffer) override {
    {
      Mutex::Autolock lock(mMutex);
      mAcquiredTextures.remove(aBuffer->GetSerial());
    }
    GonkNativeWindow::returnBuffer(aBuffer);
    // Notify codec to release an output buffer into BufferQueue.
    sp<AMessage> msg = mReleaseOutput->dup();
    msg->post();
  }

  void onFrameAvailable(const ::android::BufferItem& aItem) override {
    GonkNativeWindow::onFrameAvailable(aItem);
    // Acquire an buffer from BufferQueue and send it to codec.
    RefPtr<TextureClient> texture = getCurrentBuffer();
    Mutex::Autolock lock(mMutex);
    ProcessOutputMessages(texture);
  }

  void ProcessOutputMessages(TextureClient* aTexture) {
    RefPtr<TextureClient> texture = aTexture;
    while (!mOutputMessages.empty()) {
      sp<AMessage> msg = mOutputMessages.front();
      // We need to attach the pending texture to the message that contains
      // inputInfo. If there is no pending texture, only process the messages
      // without inputInfo.
      if (msg->contains("inputInfo")) {
        if (!texture) {
          return;
        }
        msg->setObject("texture", TextureHolder::Create(texture));
        msg->setInt64("textureSerial", texture->GetSerial());  // for debugging
        mAcquiredTextures.push_back(texture->GetSerial());
        texture = nullptr;
      }
      mOutputMessages.pop_front();
      if (!msg->contains("canceled")) {
        msg->post();
      }
    }
  }

  int GetBufferMessageCount() {
    int count = 0;
    for (auto msg : mOutputMessages) {
      count += msg->contains("inputInfo");
    }
    return count;
  }

  const sp<AMessage> mReleaseOutput;
  const int mMaxAcquiredCount;
  std::list<sp<AMessage>> mOutputMessages;
  std::list<uint64_t> mAcquiredTextures;
};

// InputInfoQueue is used to store the input metadata, which is opaque to us.
// When an output buffer is ready, the corresponding input metadata is retrieved
// from this queue and both of them are sent to the client.
class GonkMediaCodec::InputInfoQueue {
 public:
  void Push(int64_t aTimeUs, const sp<RefBase>& aInfo) {
    mQueue.push({aTimeUs, aInfo});
  }

  sp<RefBase> Find(int64_t aTimeUs) {
    if (mQueue.empty()) {
      return nullptr;
    }

    Item item;
    while (!mQueue.empty() && mQueue.top().mTime < aTimeUs) {
      item = mQueue.top();
      mQueue.pop();
    }
    if (mQueue.top().mTime == aTimeUs) {
      return mQueue.top().mInfo;
    } else {
      // No matching timestamp, just return the last info which has a smaller
      // timestamp and keep it in the queue. This may happen on audio decoder,
      // because each input info doesn't necessarily maps to an output buffer.
      mQueue.push(item);
      return item.mInfo;
    }
  }

  void Clear() { mQueue = {}; }

 private:
  struct Item {
    int64_t mTime = INT64_MIN;
    sp<RefBase> mInfo;
  };

  class Compare {
   public:
    bool operator()(const Item& aFirst, const Item& aSecond) {
      return aFirst.mTime > aSecond.mTime;
    }
  };

  std::priority_queue<Item, std::vector<Item>, Compare> mQueue;
};

GonkMediaCodec::GonkMediaCodec() {
  LOGD("%p constructor", this);

  ProcessState::self()->startThreadPool();
  mLooper = new ALooper;
  mLooper->setName("GonkMediaCodec");
  mCodecLooper = new ALooper;
  mCodecLooper->setName("GonkMediaCodec/Codec");
}

GonkMediaCodec::~GonkMediaCodec() { LOGD("%p destructor", this); }

void GonkMediaCodec::Init() {
  LOGD("%p initializing", this);
  mLooper->registerHandler(this);
  mLooper->start();
  mCodecLooper->start();
}

void GonkMediaCodec::Configure(const sp<Reply>& aReply,
                               const sp<Callback>& aCallback,
                               const sp<AMessage>& aFormat,
                               const sp<ICrypto>& aCrypto, bool aEncoder) {
  LOGD("%p configuring", this);

  sp<AMessage> msg = new AMessage(kWhatConfigure, this);
  msg->setObject("reply", aReply);
  msg->setObject("callback", aCallback);
  msg->setMessage("format", aFormat);
  msg->setObject("crypto", CryptoHolder::Create(aCrypto));
  msg->setInt32("encoder", aEncoder);
  msg->post();
}

void GonkMediaCodec::Shutdown(const sp<Reply>& aReply) {
  LOGD("%p shutting down", this);
  sp<AMessage> msg = new AMessage(kWhatShutdown, this);
  msg->setObject("reply", aReply);
  msg->post();
}

void GonkMediaCodec::Flush(const sp<Reply>& aReply) {
  LOGD("%p flushing", this);
  sp<AMessage> msg = new AMessage(kWhatFlush, this);
  msg->setObject("reply", aReply);
  msg->post();
}

void GonkMediaCodec::InputUpdated() {
  sp<AMessage> msg = new AMessage(kWhatInputUpdated, this);
  msg->post();
}

void GonkMediaCodec::onMessageReceived(const sp<AMessage>& aMsg) {
  if (IsVerboseLoggingEnabled()) {
    auto lines = SplitMultilineString(aMsg->debugString().c_str());
    LOGV("%p onMessage:", this);
    for (auto& line : lines) {
      LOGV("%p   %s", this, line.c_str());
    }
  }

  switch (aMsg->what()) {
    case kWhatCodecNotify: {
      int32_t cbID;
      CHECK(aMsg->findInt32("callbackID", &cbID));

      switch (cbID) {
        case MediaCodec::CB_INPUT_AVAILABLE: {
          int32_t index;
          CHECK(aMsg->findInt32("index", &index));
          mInputBuffers.push_back(index);
          OnInputUpdated();
          break;
        }

        case MediaCodec::CB_OUTPUT_AVAILABLE: {
          if (mNativeWindow) {
            mOutputBuffers.push_back(aMsg);
            OnReleaseOutput();
            break;
          }

          int32_t index;
          size_t offset;
          size_t size;
          int64_t timeUs;
          int32_t flags;

          CHECK(aMsg->findInt32("index", &index));
          CHECK(aMsg->findSize("offset", &offset));
          CHECK(aMsg->findSize("size", &size));
          CHECK(aMsg->findInt64("timeUs", &timeUs));
          CHECK(aMsg->findInt32("flags", &flags));
          OnOutputAvailable(index, offset, size, timeUs, flags);
          break;
        }

        case MediaCodec::CB_OUTPUT_FORMAT_CHANGED: {
          sp<AMessage> format;
          CHECK(aMsg->findMessage("format", &format));
          auto lines = SplitMultilineString(format->debugString().c_str());
          LOGI("%p format changed:", this);
          for (auto& line : lines) {
            LOGI("%p   %s", this, line.c_str());
          }
          if (mNativeWindow) {
            // Delay notifying new format until all old frames have been pulled
            // from the BufferQueue.
            sp<AMessage> notify = new AMessage(kWhatNotifyOutput, this);
            notify->setMessage("format", format);
            mNativeWindow->QueueOutputMessage(notify);
          } else {
            mCallback->NotifyOutputFormat(format);
          }
          break;
        }

        case MediaCodec::CB_ERROR: {
          int32_t err, actionCode;
          CHECK(aMsg->findInt32("err", &err));
          CHECK(aMsg->findInt32("actionCode", &actionCode));
          LOGE("%p codec error: 0x%x, actionCode %d", this, err, actionCode);
          mCallback->NotifyError(err, actionCode);
          break;
        }

        default: {
          TRESPASS();
          break;
        }
      }

      break;
    }

    case kWhatConfigure: {
      if (mConfigMsg || mCodec) {
        LOGE("%p already configured", this);
        sp<RefBase> obj;
        CHECK(aMsg->findObject("reply", &obj));
        static_cast<Reply*>(obj.get())->Invoke(INVALID_OPERATION);
        break;
      }
      mConfigMsg = aMsg;
      sp<AMessage> format;
      int32_t encoder;
      CHECK(aMsg->findMessage("format", &format));
      CHECK(aMsg->findInt32("encoder", &encoder));
      ReserveResource(format, encoder);
      break;
    }

    case kWhatResourceReserved: {
      sp<Reply> reply;
      sp<Callback> callback;
      sp<AMessage> format;
      sp<ICrypto> crypto;
      int32_t encoder;
      CHECK(mConfigMsg->findMessage("format", &format));
      CHECK(mConfigMsg->findInt32("encoder", &encoder));

      sp<RefBase> obj;
      CHECK(mConfigMsg->findObject("reply", &obj));
      reply = static_cast<Reply*>(obj.get());
      CHECK(mConfigMsg->findObject("callback", &obj));
      callback = static_cast<Callback*>(obj.get());
      CHECK(mConfigMsg->findObject("crypto", &obj));
      crypto = static_cast<CryptoHolder*>(obj.get())->Get();
      mConfigMsg = nullptr;

      status_t err = OnConfigure(callback, format, crypto, encoder);
      if (err != OK) {
        OnShutdown();
      }
      reply->Invoke(err);
      break;
    }

    case kWhatResourceReservationFailed: {
      sp<RefBase> obj;
      CHECK(mConfigMsg->findObject("reply", &obj));
      static_cast<Reply*>(obj.get())->Invoke(UNKNOWN_ERROR);
      mConfigMsg = nullptr;
      break;
    }

    case kWhatInputUpdated: {
      OnInputUpdated();
      break;
    }

    case kWhatReleaseOutput: {
      OnReleaseOutput();
      break;
    }

    case kWhatNotifyOutput: {
      OnNotifyOutput(aMsg);
      break;
    }

    case kWhatFlush: {
      sp<Reply> reply;
      sp<RefBase> obj;
      CHECK(aMsg->findObject("reply", &obj));
      reply = static_cast<Reply*>(obj.get());

      status_t err = OnFlush();
      reply->Invoke(err);
      break;
    }

    case kWhatShutdown: {
      sp<Reply> reply;
      sp<RefBase> obj;
      CHECK(aMsg->findObject("reply", &obj));
      reply = static_cast<Reply*>(obj.get());

      status_t err = OnShutdown();
      reply->Invoke(err);
      break;
    }

    case kWhatPrintBufferQueueStats: {
      if (mNativeWindow) {
        mNativeWindow->PrintStats();
        sp<AMessage> msg = new AMessage(kWhatPrintBufferQueueStats, this);
        msg->post(1000000);
      }
      break;
    }
  }
}

status_t GonkMediaCodec::OnConfigure(const sp<Callback>& aCallback,
                                     const sp<AMessage>& aFormat,
                                     const sp<ICrypto>& aCrypto,
                                     bool aEncoder) {
  CHECK(aCallback);
  mCallback = aCallback;
  mInputInfoQueue.reset(new InputInfoQueue);

  AString mime;
  CHECK(aFormat->findString("mime", &mime));

  sp<Surface> surface;
  if (mime.startsWith("video/") &&
      mozilla::StaticPrefs::media_gonkmediacodec_bufferqueue_enabled()) {
    surface = InitBufferQueue();
    if (!surface) {
      LOGE("%p failed to init buffer queue", this);
      return UNKNOWN_ERROR;
    }
  }

  auto matches = GonkMediaUtils::FindMatchingCodecs(mime.c_str(), aEncoder);
  for (auto& name : matches) {
    mCodec = MediaCodec::CreateByComponentName(mCodecLooper, name);
    if (mCodec) {
      LOGI("%p codec created: %s", this, name.c_str());
      break;
    }
  }
  if (!mCodec) {
    LOGE("%p failed to create codec", this);
    return UNKNOWN_ERROR;
  }

  status_t err = mCodec->configure(aFormat, surface, aCrypto, 0);
  if (err != OK) {
    LOGE("%p failed to configure codec", this);
    return UNKNOWN_ERROR;
  }

  // Run MediaCodec in async mode.
  sp<AMessage> reply = new AMessage(kWhatCodecNotify, this);
  err = mCodec->setCallback(reply);
  if (err != OK) {
    LOGE("%p failed to set codec callback", this);
    return UNKNOWN_ERROR;
  }

  err = mCodec->start();
  if (err != OK) {
    LOGE("%p failed to start codec", this);
    return UNKNOWN_ERROR;
  }

  sp<MediaCodecInfo> codecInfo;
  err = mCodec->getCodecInfo(&codecInfo);
  if (err == OK) {
    if (auto caps = codecInfo->getCapabilitiesFor(mime.c_str())) {
      auto details = caps->getDetails();
      auto lines = SplitMultilineString(details->debugString().c_str());
      LOGI("%p codec details for %s:", this, codecInfo->getCodecName());
      for (auto& line : lines) {
        LOGI("%p   %s", this, line.c_str());
      }
      mCallback->NotifyCodecDetails(details);
    }
  }

  if (mNativeWindow &&
      mozilla::StaticPrefs::media_gonkmediacodec_bufferqueue_debug()) {
    sp<AMessage> msg = new AMessage(kWhatPrintBufferQueueStats, this);
    msg->post(1000000);
  }
  return OK;
}

status_t GonkMediaCodec::OnShutdown() {
  if (mCodec) {
    mCodec->release();
    mCodec = nullptr;
  }
  if (mNativeWindow) {
    mNativeWindow->Release();
    mNativeWindow = nullptr;
  }
  if (mResourceClient) {
    mResourceClient->ReleaseResource();
    mResourceClient = nullptr;
  }
  mLooper->stop();
  mLooper->unregisterHandler(id());
  mInputBuffers.clear();
  mOutputBuffers.clear();
  mInputInfoQueue = nullptr;
  mCallback = nullptr;
  mConfigMsg = nullptr;
  return OK;
}

status_t GonkMediaCodec::OnFlush() {
  status_t err = mCodec->flush();
  if (err != OK) {
    LOGE("%p failed to flush codec", this);
  }

  mInputBuffers.clear();
  mOutputBuffers.clear();
  mInputInfoQueue->Clear();
  if (mNativeWindow) {
    mNativeWindow->CancelPendingOutputMessages();
  }

  err = mCodec->start();
  if (err != OK) {
    LOGE("%p failed to start codec", this);
  }
  return err;
}

void GonkMediaCodec::OnInputUpdated() {
  while (mInputBuffers.size()) {
    auto index = mInputBuffers.front();
    mInputBuffers.pop_front();

    sp<MediaCodecBuffer> buffer;
    mCodec->getInputBuffer(index, &buffer);
    if (!buffer) {
      LOGE("%p failed to get input buffer", this);
      continue;
    }

    int64_t timeUs = INT64_MIN;
    uint32_t flags = 0;
    sp<RefBase> inputInfo;
    sp<GonkCryptoInfo> cryptoInfo;
    if (!mCallback->FetchInput(buffer, &inputInfo, &cryptoInfo, &timeUs,
                               &flags)) {
      mInputBuffers.push_front(index);
      break;
    }

    status_t err;
    if (!cryptoInfo || cryptoInfo->mMode == CryptoPlugin::kMode_Unencrypted) {
      err = mCodec->queueInputBuffer(index, buffer->offset(), buffer->size(),
                                     timeUs, flags);
    } else {
      err = mCodec->queueSecureInputBuffer(
          index, buffer->offset(), cryptoInfo->mSubSamples.data(),
          cryptoInfo->mSubSamples.size(), cryptoInfo->mKey.data(),
          cryptoInfo->mIV.data(), cryptoInfo->mMode, cryptoInfo->mPattern,
          timeUs, flags);
    }
    if (err != OK) {
      LOGE("%p failed to queue input buffer", this);
      continue;
    }

    mInputInfoQueue->Push(timeUs, inputInfo);
  }
}

void GonkMediaCodec::OnOutputAvailable(int32_t aIndex, size_t aOffset,
                                       size_t aSize, int64_t aTimeUs,
                                       int32_t aFlags) {
  CHECK(!mNativeWindow);
  sp<MediaCodecBuffer> buffer;
  mCodec->getOutputBuffer(aIndex, &buffer);
  if (!buffer) {
    LOGE("%p failed to get output buffer %d", this, aIndex);
    return;
  }

  buffer->setRange(aOffset, aSize);
  mCallback->Output(buffer, mInputInfoQueue->Find(aTimeUs), aTimeUs, aFlags);
  mCodec->releaseOutputBuffer(aIndex);
}

void GonkMediaCodec::OnReleaseOutput() {
  CHECK(mNativeWindow);
  while (mOutputBuffers.size() && mNativeWindow->CanAcquire()) {
    sp<AMessage> msg = mOutputBuffers.front();
    mOutputBuffers.pop_front();

    int32_t index, flags;
    int64_t timeUs;
    size_t size;
    CHECK(msg->findInt32("index", &index));
    CHECK(msg->findSize("size", &size));
    CHECK(msg->findInt64("timeUs", &timeUs));
    CHECK(msg->findInt32("flags", &flags));

    sp<MediaCodecBuffer> buffer;
    mCodec->getOutputBuffer(index, &buffer);
    if (!buffer) {
      LOGE("%p failed to get output buffer %d", this, index);
      continue;
    }

    sp<AMessage> notify = new AMessage(kWhatNotifyOutput, this);
    notify->setInt64("timeUs", timeUs);
    notify->setInt32("flags", flags);
    if (size > 0) {
      // MediaCodec may notify EOS flag with an empty buffer. In this case,
      // don't set inputInfo so CodecNativeWindow won't try to acquire a
      // texture for it.
      notify->setObject("inputInfo", mInputInfoQueue->Find(timeUs));
    }
    mNativeWindow->QueueOutputMessage(notify);
    status_t err = mCodec->renderOutputBufferAndRelease(index);
    CHECK_EQ(OK, err);
  }
}

void GonkMediaCodec::OnNotifyOutput(const sp<AMessage>& aMsg) {
  CHECK(mNativeWindow);
  if (aMsg->contains("format")) {
    sp<AMessage> format;
    CHECK(aMsg->findMessage("format", &format));
    mCallback->NotifyOutputFormat(format);
  }
  if (aMsg->contains("flags")) {
    int64_t timeUs;
    int32_t flags;
    sp<RefBase> inputInfo;
    RefPtr<TextureClient> texture;
    CHECK(aMsg->findInt64("timeUs", &timeUs));
    CHECK(aMsg->findInt32("flags", &flags));
    if (aMsg->contains("inputInfo")) {
      sp<RefBase> obj;
      CHECK(aMsg->findObject("inputInfo", &inputInfo));
      CHECK(aMsg->findObject("texture", &obj));
      texture = static_cast<TextureHolder*>(obj.get())->Get();
    }
    mCallback->Output(texture, inputInfo, timeUs, flags);
  }
}

sp<Surface> GonkMediaCodec::InitBufferQueue() {
  LOGI("%p creating GonkBufferQueue", this);
  sp<IGraphicBufferProducer> producer;
  sp<IGonkGraphicBufferConsumer> consumer;
  GonkBufferQueue::createBufferQueue(&producer, &consumer);

  // Set how many graphic buffers can be acquired at consumer side.
  int maxAcquiredCount = mozilla::StaticPrefs::
      media_gonkmediacodec_bufferqueue_max_acquired_count();

  mNativeWindow = new CodecNativeWindow(
      consumer, new AMessage(kWhatReleaseOutput, this), maxAcquiredCount);
  return new Surface(producer);
}

void GonkMediaCodec::ReserveResource(const sp<AMessage>& aFormat,
                                     bool aEncoder) {
  AString mime;
  CHECK(aFormat->findString("mime", &mime));
  bool isVideo = mime.startsWith("video/");

  mozilla::MediaSystemResourceType type;
  if (aEncoder) {
    type = isVideo ? mozilla::MediaSystemResourceType::VIDEO_ENCODER
                   : mozilla::MediaSystemResourceType::AUDIO_ENCODER;
  } else {
    type = isVideo ? mozilla::MediaSystemResourceType::VIDEO_DECODER
                   : mozilla::MediaSystemResourceType::AUDIO_DECODER;
  }

  LOGD("%p reserving resource type %d", this, int(type));

  // Currently only video supports resource management.
  if (!isVideo) {
    ResourceReserved();
    return;
  }

  mResourceClient = new mozilla::MediaSystemResourceClient(type);
  mResourceClient->SetListener(this);
  mResourceClient->Acquire();
}

void GonkMediaCodec::ResourceReserved() {
  LOGD("%p resource reserved", this);
  sp<AMessage> msg = new AMessage(kWhatResourceReserved, this);
  msg->post();
}

void GonkMediaCodec::ResourceReserveFailed() {
  LOGE("%p resource reservation failed", this);
  sp<AMessage> msg = new AMessage(kWhatResourceReservationFailed, this);
  msg->post();
}

}  // namespace android
