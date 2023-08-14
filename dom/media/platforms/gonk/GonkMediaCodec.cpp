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

class GonkMediaCodec::CodecNativeWindow final : public GonkNativeWindow {
  using TextureClient = mozilla::layers::TextureClient;

 public:
  CodecNativeWindow(const sp<GonkMediaCodec>& aCodec,
                    const sp<IGonkGraphicBufferConsumer>& aConsumer,
                    int aMaxAcquiredCount)
      : GonkNativeWindow(aConsumer),
        mCodec(aCodec),
        mMaxAcquiredCount(aMaxAcquiredCount) {
    mConsumer->setMaxAcquiredBufferCount(mMaxAcquiredCount);
  }

  already_AddRefed<TextureClient> GetCurrentFrame(int64_t* aTimeNs,
                                                  bool aFlushing) {
    *aTimeNs = INT64_MIN;

    // GonkBufferQueue allows the max acquired buffer count to be exceeded by
    // one. Here we reserve that one for flushing.
    int maxAcquiredCount = mMaxAcquiredCount + aFlushing;
    if (mConsumer->getAcquiredBufferCount() >= maxAcquiredCount) {
      return nullptr;
    }

    RefPtr<TextureClient> textureClient;
    {
      Mutex::Autolock _l(mMutex);
      BufferItem item;

      // In asynchronous mode the list is guaranteed to be one buffer
      // deep, while in synchronous mode we use the oldest buffer.
      status_t err = acquireBufferLocked(&item, 0);
      if (err != NO_ERROR) {
        return nullptr;
      }

      textureClient =
          mConsumer->getTextureClientFromBuffer(item.mGraphicBuffer.get());
      if (!textureClient) {
        return nullptr;
      }
      textureClient->SetRecycleCallback(GonkNativeWindow::RecycleCallback,
                                        static_cast<GonkNativeWindow*>(this));

      // Recover the corresponding timestamp.
      while (!mBufferInfos.empty()) {
        auto [number, timestamp] = mBufferInfos.front();
        mBufferInfos.pop_front();
        if (item.mFrameNumber == static_cast<uint64_t>(number)) {
          *aTimeNs = timestamp;
          break;
        }
      }
    }

    if (*aTimeNs == INT64_MIN) {
      return nullptr;
    }
    return textureClient.forget();
  }

 private:
  status_t releaseBufferLocked(int slot,
                               const sp<GraphicBuffer> graphicBuffer) override {
    status_t err = GonkConsumerBase::releaseBufferLocked(slot, graphicBuffer);
    if (auto codec = mCodec.promote()) {
      // A frame has just been returned to GonkBufferQueue. Signal the codec to
      // get queued frames, in case we have reached max acquired buffer count
      // and can't get those frames earlier.
      codec->BufferQueueUpdated();
    }
    return err;
  }

  void onFrameAvailable(const ::android::BufferItem& aItem) override {
    GonkConsumerBase::onFrameAvailable(aItem);
    {
      Mutex::Autolock lock(mMutex);
      auto& item = reinterpret_cast<const GonkBufferItem&>(aItem);
      // Store the mapping between frame number and timestamp. We will use this
      // information to recover the timestamp of each GraphicBuffer in
      // GetCurrentFrame().
      mBufferInfos.push_back({item.mFrameNumber, item.mTimestamp});
    }
    if (auto codec = mCodec.promote()) {
      // Signal the codec to get the queued frames.
      codec->BufferQueueUpdated();
    }
  }

  wp<GonkMediaCodec> mCodec;
  const int mMaxAcquiredCount;
  std::deque<std::pair<int64_t, int64_t>> mBufferInfos;
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

// When using BufferQueue, OutputInfoQueue is used to make sure each frame, new
// output format and EOS flag are notified in correct order.
class GonkMediaCodec::OutputInfoQueue {
 public:
  void Push(int64_t aTimeUs) {
    CHECK(!mIsFinished);
    mQueue.push_back({aTimeUs, mPendingFormat});
    mPendingFormat = nullptr;
  }

  void PushFormat(const sp<AMessage>& aFormat) {
    CHECK(aFormat);
    CHECK(!mIsFinished);
    mPendingFormat = aFormat;
  }

  void Finish() { mIsFinished = true; }

  bool AtEndOfStream() { return mIsFinished && mQueue.empty(); }

  sp<AMessage> Find(int64_t aTimeUs) {
    sp<AMessage> lastFormat;
    while (!mQueue.empty() && mQueue.front().first <= aTimeUs) {
      if (auto format = mQueue.front().second) {
        // Overwrite any outdated format.
        lastFormat = format;
      }
      mQueue.pop_front();
    }
    return lastFormat;
  }

  void Clear() {
    mQueue.clear();
    mPendingFormat = nullptr;
    mIsFinished = false;
  }

 private:
  std::deque<std::pair<int64_t, sp<AMessage>>> mQueue;
  sp<AMessage> mPendingFormat;
  bool mIsFinished = false;
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

// We can't downcast RefBase to ICrypto due to virtual inheritance. This means
// an ICrypto object can't be retrieved from AMessage::findObject(). Instead we
// can use this wrapper to set an ICrypto object into AMessage while still
// holding a reference to it.
struct CryptoHolder : public RefBase {
  sp<ICrypto> mCrypto;

  static sp<CryptoHolder> Create(const sp<ICrypto>& aCrypto) {
    sp<CryptoHolder> holder = new CryptoHolder;
    holder->mCrypto = aCrypto;
    return holder;
  }
};

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

void GonkMediaCodec::BufferQueueUpdated() {
  sp<AMessage> msg = new AMessage(kWhatBufferQueueUpdated, this);
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
            mOutputInfoQueue->PushFormat(format);
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
      crypto = static_cast<CryptoHolder*>(obj.get())->mCrypto;
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

    case kWhatBufferQueueUpdated: {
      OnBufferQueueUpdated();
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
    mOutputInfoQueue.reset(new OutputInfoQueue);
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
  return OK;
}

status_t GonkMediaCodec::OnShutdown() {
  if (mCodec) {
    mCodec->release();
    mCodec = nullptr;
  }
  if (mNativeWindow) {
    mNativeWindow->abandon();
    mNativeWindow = nullptr;
  }
  if (mResourceClient) {
    mResourceClient->ReleaseResource();
    mResourceClient = nullptr;
  }
  mLooper->stop();
  mLooper->unregisterHandler(id());
  mInputBuffers.clear();
  mInputInfoQueue = nullptr;
  mOutputInfoQueue = nullptr;
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
  mInputInfoQueue->Clear();
  if (mOutputInfoQueue) {
    mOutputInfoQueue->Clear();
  }

  // Flush BufferQueue.
  if (mNativeWindow) {
    RefPtr<TextureClient> buffer;
    int64_t timeNs;
    int frameCnt = 0;
    while ((buffer = mNativeWindow->GetCurrentFrame(&timeNs, true))) {
      frameCnt++;
    }
    LOGD("%p flushed %d graphic buffers", this, frameCnt);
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
  bool eos = aFlags & MediaCodec::BUFFER_FLAG_EOS;

  if (mNativeWindow) {
    CHECK(mOutputInfoQueue);
    // Will get graphic buffer in OnBufferQueueUpdated().
    mCodec->renderOutputBufferAndRelease(aIndex);
    if (aSize > 0) {
      mOutputInfoQueue->Push(aTimeUs);
    }
    if (eos) {
      mOutputInfoQueue->Finish();
    }
    // Notify EOS now if there is no more frames to be output.
    if (mOutputInfoQueue->AtEndOfStream()) {
      mCallback->NotifyOutputEnded();
    }
    return;
  }

  sp<MediaCodecBuffer> buffer;
  mCodec->getOutputBuffer(aIndex, &buffer);
  if (!buffer) {
    LOGE("%p failed to get output buffer", this);
    return;
  }

  if (aSize > 0) {
    buffer->setRange(aOffset, aSize);
    mCallback->Output(buffer, mInputInfoQueue->Find(aTimeUs), aTimeUs);
  }
  mCodec->releaseOutputBuffer(aIndex);
  if (eos) {
    mCallback->NotifyOutputEnded();
  }
}

void GonkMediaCodec::OnBufferQueueUpdated() {
  CHECK(mNativeWindow);
  CHECK(mOutputInfoQueue);
  while (true) {
    int64_t timeNs = INT64_MIN;
    RefPtr<TextureClient> buffer =
        mNativeWindow->GetCurrentFrame(&timeNs, false);
    if (!buffer) {
      break;
    }

    int64_t timeUs = timeNs / 1000;
    // Notify new format before sending output buffer.
    if (auto format = mOutputInfoQueue->Find(timeUs)) {
      mCallback->NotifyOutputFormat(format);
    }
    mCallback->Output(buffer, mInputInfoQueue->Find(timeUs), timeUs);
    if (mOutputInfoQueue->AtEndOfStream()) {
      mCallback->NotifyOutputEnded();
      break;
    }
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

  mNativeWindow = new CodecNativeWindow(this, consumer, maxAcquiredCount);
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

  LOGD("%p reserving resource type %d", this, type);

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
