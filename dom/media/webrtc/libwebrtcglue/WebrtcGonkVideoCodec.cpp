/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebrtcGonkVideoCodec.h"

#include <binder/ProcessState.h>
#include <gui/Surface.h>
#include <media/MediaCodecBuffer.h>
#include <media/stagefright/foundation/ABuffer.h>
#include <media/stagefright/foundation/AMessage.h>
#include <mediadrm/ICrypto.h>
#include <OMX_IVCommon.h>

#include "GonkBufferQueueProducer.h"
#include "GrallocImages.h"
#include "libyuv.h"
#include "WebrtcImageBuffer.h"

#if defined(PRODUCT_MANUFACTURER_MTK)
#  define WEBRTC_OMX_MIN_DECODE_BUFFERS 4
#else
#  define WEBRTC_OMX_MIN_DECODE_BUFFERS 10
#endif

namespace android {

#undef LOGE
#undef LOGW
#undef LOGI
#undef LOGD
#undef LOGV

static mozilla::LazyLogModule sCodecLog("WebrtcGonkVideoCodec");
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

struct FrameInfo : public RefBase {
  bool mIsCodecConfig = false;
  uint32_t mWidth = 0;
  uint32_t mHeight = 0;
  uint32_t mTimestamp = 0;
  int64_t mTimestampUs = 0;
  int64_t mRenderTimeMs = 0;

  // Debug info
  int64_t mInputTimeNs = 0;
  ANativeWindowBuffer* mNativeBuffer = 0;
};

void FrameInfoQueue::SetOwner(void* aOwner, const char* aTag) {
  mOwner = aOwner;
  mTag = aTag;
}

void FrameInfoQueue::Push(const sp<FrameInfo>& aFrameInfo) {
  Mutex::Autolock lock(mMutex);
  mQueue.push_back(aFrameInfo);
  if (mQueue.size() > 32) {
    LOGE("%s:%p FrameInfoQueue has too many frames, dropping an old one",
         mTag.c_str(), mOwner);
    mQueue.pop_front();
  }
}

sp<FrameInfo> FrameInfoQueue::Pop(int64_t aTimestampUs) {
  Mutex::Autolock lock(mMutex);

  if (mQueue.empty()) {
    LOGE("%s:%p FrameInfoQueue is empty, return null", mTag.c_str(), mOwner);
    return nullptr;
  }

  // Find the target frame and clear the frames before it.
  sp<FrameInfo> frameInfo;
  do {
    frameInfo = mQueue.front();
    mQueue.pop_front();
  } while (!mQueue.empty() && mQueue.front()->mTimestampUs <= aTimestampUs);

  if (frameInfo->mTimestampUs != aTimestampUs) {
    LOGE("%s:%p FrameInfoQueue has no matched frame, return a nearby one",
         mTag.c_str(), mOwner);
  }

  // Keep the found frame in the queue, in case the next Pop() maps to the same
  // frame.
  mQueue.push_front(frameInfo);
  return frameInfo;
}

class WebrtcGonkVideoEncoder::FrameBufferGrip : public RefBase {
 public:
  FrameBufferGrip(
      const rtc::scoped_refptr<webrtc::VideoFrameBuffer>& aFrameBuffer)
      : mFrameBuffer(aFrameBuffer) {}

  rtc::scoped_refptr<webrtc::VideoFrameBuffer> Get() { return mFrameBuffer; }

 private:
  ~FrameBufferGrip() = default;

  rtc::scoped_refptr<webrtc::VideoFrameBuffer> mFrameBuffer;
};

WebrtcGonkVideoEncoder::WebrtcGonkVideoEncoder() {
  LOGD("Encoder:%p constructor", this);

  ProcessState::self()->startThreadPool();
  mEncoderLooper = new ALooper;
  mEncoderLooper->setName("WebrtcGonkVideoEncoder");
  mCodecLooper = new ALooper;
  mCodecLooper->setName("WebrtcGonkVideoEncoder/Codec");

  mFrameInfoQueue.SetOwner(this, "Encoder");
}

WebrtcGonkVideoEncoder::~WebrtcGonkVideoEncoder() {
  LOGD("Encoder:%p destructor", this);
  Release();
}

status_t WebrtcGonkVideoEncoder::Init(Callback* aCallback, const char* aMime) {
  LOGD("Encoder:%p initializing, mime:%s", this, aMime);

  mCallback = aCallback;

  mEncoderLooper->registerHandler(this);
  mEncoderLooper->start();
  mCodecLooper->start();

  mCodec = MediaCodec::CreateByType(mCodecLooper, aMime, true);
  if (!mCodec) {
    return UNKNOWN_ERROR;
  }

  // Delay calling mCodec->start() until configured.
  return OK;
}

status_t WebrtcGonkVideoEncoder::Release() {
  LOGD("Encoder:%p releasing", this);

  mEncoderLooper->stop();
  mEncoderLooper->unregisterHandler(id());

  if (mCodec) {
    mCodec->release();
    mCodec = nullptr;
  }
  mInputFrames.clear();
  mInputBuffers.clear();
  mCallback = nullptr;
  return OK;
}

status_t WebrtcGonkVideoEncoder::Configure(const sp<AMessage>& aFormat) {
  if (!mCodec) {
    return INVALID_OPERATION;
  }

  auto lines = SplitMultilineString(aFormat->debugString().c_str());
  for (auto& line : lines) {
    LOGI("Encoder:%p configure: %s", this, line.c_str());
  }

  // QM215 encoder only supports NV12 color format.
  CHECK(aFormat->findInt32("color-format", &mColorFormat));
  CHECK(mColorFormat == OMX_COLOR_FormatYUV420SemiPlanar);

  sp<AMessage> msg = new AMessage(kWhatConfigure, this);
  msg->setMessage("format", aFormat);
  msg->post();
  return OK;
}

status_t WebrtcGonkVideoEncoder::Encode(const webrtc::VideoFrame& aInputImage) {
  if (!mCodec) {
    return INVALID_OPERATION;
  }

  sp<FrameInfo> frameInfo = new FrameInfo();
  frameInfo->mWidth = aInputImage.width();
  frameInfo->mHeight = aInputImage.height();
  frameInfo->mTimestamp = aInputImage.timestamp();
  frameInfo->mTimestampUs = aInputImage.timestamp_us();
  frameInfo->mRenderTimeMs = aInputImage.render_time_ms();
  frameInfo->mInputTimeNs = systemTime();

  sp<FrameBufferGrip> frameData =
      new FrameBufferGrip(aInputImage.video_frame_buffer());

  sp<AMessage> msg = new AMessage(kWhatQueueInputData, this);
  msg->setObject("frame-info", frameInfo.get());
  msg->setObject("frame-data", frameData.get());
  msg->post();
  return OK;
}

status_t WebrtcGonkVideoEncoder::RequestIDRFrame() {
  if (!mCodec) {
    return INVALID_OPERATION;
  }
  LOGV("WebrtcGonkVideoDecoder:%p requesting IDR frame", this);
  return mCodec->requestIDRFrame();
}

status_t WebrtcGonkVideoEncoder::SetBitrate(int32_t aBps) {
  if (!mCodec) {
    return INVALID_OPERATION;
  }
  LOGV("WebrtcGonkVideoDecoder:%p setting bitrate to %d bps", this, aBps);
  sp<AMessage> msg = new AMessage();
  msg->setInt32("video-bitrate", aBps);
  return mCodec->setParameters(msg);
}

void WebrtcGonkVideoEncoder::onMessageReceived(const sp<AMessage>& aMsg) {
  if (IsVerboseLoggingEnabled()) {
    auto lines = SplitMultilineString(aMsg->debugString().c_str());
    for (auto& line : lines) {
      LOGV("WebrtcGonkVideoDecoder:%p onMessage: %s", this, line.c_str());
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
          OnFillInputBuffers();
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

          OnDrainOutputBuffer(index, offset, size, timeUs, flags);
          break;
        }

        case MediaCodec::CB_OUTPUT_FORMAT_CHANGED: {
          sp<AMessage> format;
          CHECK(aMsg->findMessage("format", &format));
          auto lines = SplitMultilineString(format->debugString().c_str());
          for (auto& line : lines) {
            LOGI("Encoder:%p format changed: %s", this, line.c_str());
          }
          break;
        }

        case MediaCodec::CB_ERROR: {
          status_t err;
          CHECK(aMsg->findInt32("err", &err));
          LOGE("Encoder:%p reported error: 0x%x", this, err);
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
      sp<AMessage> format;
      CHECK(aMsg->findMessage("format", &format));
      OnConfigure(format);
      break;
    }

    case kWhatQueueInputData: {
      sp<RefBase> obj;
      CHECK(aMsg->findObject("frame-info", &obj));
      sp<FrameInfo> frameInfo = static_cast<FrameInfo*>(obj.get());
      CHECK(aMsg->findObject("frame-data", &obj));
      sp<FrameBufferGrip> frameData = static_cast<FrameBufferGrip*>(obj.get());

      mInputFrames.push_back(std::make_pair(frameInfo, frameData));
      OnFillInputBuffers();
      break;
    }
  }
}

void WebrtcGonkVideoEncoder::OnConfigure(const sp<AMessage>& aFormat) {
  mInputBuffers.clear();

  if (mStarted) {
    if (mCodec->stop() != OK) {
      LOGE("Encoder:%p failed to stop codec", this);
      return;
    }
    mStarted = false;
  }

  if (mCodec->configure(aFormat, nullptr, nullptr,
                        MediaCodec::CONFIGURE_FLAG_ENCODE) != OK) {
    LOGE("Encoder:%p failed to configure codec", this);
    return;
  }

  // Run MediaCodec in async mode.
  sp<AMessage> reply = new AMessage(kWhatCodecNotify, this);
  if (mCodec->setCallback(reply) != OK) {
    LOGE("Encoder:%p failed to set codec callback", this);
    return;
  }

  if (mCodec->start() != OK) {
    LOGE("Encoder:%p failed to start codec", this);
    return;
  }

  mStarted = true;
}

static status_t ConvertYUV(android_ycbcr& aDstYUV, PixelFormat aDstFormat,
                           android_ycbcr& aSrcYUV, PixelFormat aSrcFormat,
                           int aWidth, int aHeight) {
  CHECK(aDstFormat == OMX_COLOR_FormatYUV420SemiPlanar);

  int result = -1;
  switch (aSrcFormat) {
    case HAL_PIXEL_FORMAT_YV12:
      result = libyuv::I420ToNV12(
          static_cast<uint8_t*>(aSrcYUV.y), static_cast<int>(aSrcYUV.ystride),
          static_cast<uint8_t*>(aSrcYUV.cb), static_cast<int>(aSrcYUV.cstride),
          static_cast<uint8_t*>(aSrcYUV.cr), static_cast<int>(aSrcYUV.cstride),
          static_cast<uint8_t*>(aDstYUV.y), static_cast<int>(aDstYUV.ystride),
          static_cast<uint8_t*>(aDstYUV.cb), static_cast<int>(aDstYUV.cstride),
          aWidth, aHeight);
      break;
  }
  return result < 0 ? INVALID_OPERATION : OK;
}

static status_t ConvertFrameBuffer(
    const sp<MediaCodecBuffer>& aDst, PixelFormat aDstFormat,
    const rtc::scoped_refptr<webrtc::VideoFrameBuffer>& aSrc) {
  CHECK(aDstFormat == OMX_COLOR_FormatYUV420SemiPlanar);

  int width = aSrc->width();
  int height = aSrc->height();
  int cWidth = (width + 1) / 2;
  int cHeight = (height + 1) / 2;

  if (static_cast<int>(aDst->size()) < width * height + cWidth * cHeight * 2) {
    return INVALID_OPERATION;
  }

  android_ycbcr dstYUV = {.y = aDst->data(),
                          .cb = aDst->data() + width * height,
                          .cr = aDst->data() + width * height + 1,
                          .ystride = static_cast<size_t>(width),
                          .cstride = static_cast<size_t>(cWidth * 2),
                          .chroma_step = 2};

  status_t err;
  if (aSrc->type() == webrtc::VideoFrameBuffer::Type::kNative) {
    // Only GrallocImage is wrapped into native VideoFrameBuffer.
    auto* imageBuffer = static_cast<mozilla::ImageBuffer*>(aSrc.get());
    auto* grallocImage = imageBuffer->GetNativeImage()->AsGrallocImage();
    CHECK(grallocImage);
    sp<GraphicBuffer> graphicBuffer = grallocImage->GetGraphicBuffer();
    PixelFormat srcFormat = graphicBuffer->getPixelFormat();
    android_ycbcr srcYUV = {};

    graphicBuffer->lockYCbCr(GraphicBuffer::USAGE_SW_READ_MASK, &srcYUV);
    err = ConvertYUV(dstYUV, aDstFormat, srcYUV, srcFormat, width, height);
    graphicBuffer->unlock();
  } else {
    rtc::scoped_refptr<webrtc::I420BufferInterface> i420 = aSrc->ToI420();
    android_ycbcr srcYUV = {.y = const_cast<uint8_t*>(i420->DataY()),
                            .cb = const_cast<uint8_t*>(i420->DataU()),
                            .cr = const_cast<uint8_t*>(i420->DataV()),
                            .ystride = static_cast<size_t>(i420->StrideY()),
                            .cstride = static_cast<size_t>(i420->StrideU()),
                            .chroma_step = 1};
    err = ConvertYUV(dstYUV, aDstFormat, srcYUV, HAL_PIXEL_FORMAT_YV12, width,
                     height);
  }
  return err;
}

void WebrtcGonkVideoEncoder::OnFillInputBuffers() {
  while (mInputFrames.size() && mInputBuffers.size()) {
    auto index = mInputBuffers.front();
    mInputBuffers.pop_front();

    sp<MediaCodecBuffer> buffer;
    mCodec->getInputBuffer(index, &buffer);
    if (!buffer) {
      LOGE("Encoder:%p failed to get input buffer", this);
      continue;
    }

    auto [frameInfo, frameData] = mInputFrames.front();
    mInputFrames.pop_front();

    status_t err = ConvertFrameBuffer(buffer, mColorFormat, frameData->Get());
    if (err != OK) {
      LOGE("Encoder:%p failed to convert buffer, dropping one frame", this);
      mInputBuffers.push_front(index);  // keep the buffer
      continue;
    }

    err = mCodec->queueInputBuffer(index, 0, buffer->size(),
                                   frameInfo->mTimestampUs, 0);
    if (err != OK) {
      LOGE("Encoder:%p failed to queue input buffer", this);
      continue;
    }

    mFrameInfoQueue.Push(frameInfo);
  }
}

void WebrtcGonkVideoEncoder::OnDrainOutputBuffer(size_t aIndex, size_t aOffset,
                                                 size_t aSize, int64_t aTimeUs,
                                                 int32_t aFlags) {
  sp<MediaCodecBuffer> buffer;
  mCodec->getOutputBuffer(aIndex, &buffer);
  if (!buffer) {
    LOGE("Encoder:%p failed to get output buffer", this);
    return;
  }

  sp<FrameInfo> frameInfo = mFrameInfoQueue.Pop(aTimeUs);

  webrtc::EncodedImage encoded;
  encoded.SetEncodedData(
      webrtc::EncodedImageBuffer::Create(buffer->data(), buffer->size()));
  encoded._encodedWidth = frameInfo->mWidth;
  encoded._encodedHeight = frameInfo->mHeight;
  encoded.SetTimestamp(frameInfo->mTimestamp);
  encoded.capture_time_ms_ = frameInfo->mRenderTimeMs;

  if (aFlags & MediaCodec::BUFFER_FLAG_SYNCFRAME) {
    encoded._frameType = webrtc::VideoFrameType::kVideoFrameKey;
  } else {
    encoded._frameType = webrtc::VideoFrameType::kVideoFrameDelta;
  }

  int64_t latency = (systemTime() - frameInfo->mInputTimeNs) / 1000000;
  LOGD("Encoder:%p frame encoded, latency %" PRId64 " ms", this, latency);

  CHECK(mCallback);
  mCallback->OnEncoded(encoded);
  mCodec->releaseOutputBuffer(aIndex);
}

WebrtcGonkVideoDecoder::WebrtcGonkVideoDecoder() {
  LOGD("Decoder:%p constructor", this);

  ProcessState::self()->startThreadPool();
  mDecoderLooper = new ALooper;
  mDecoderLooper->setName("WebrtcGonkVideoDecoder");
  mCodecLooper = new ALooper;
  mCodecLooper->setName("WebrtcGonkVideoDecoder/Codec");

  mFrameInfoQueue.SetOwner(this, "Decoder");
}

WebrtcGonkVideoDecoder::~WebrtcGonkVideoDecoder() {
  LOGD("Decoder:%p destructor", this);
  Release();
}

status_t WebrtcGonkVideoDecoder::Init(Callback* aCallback, const char* aMime,
                                      int32_t aWidth, int32_t aHeight) {
  LOGD("Decoder:%p initializing, mime:%s, width:%d, height:%d", this, aMime,
       aWidth, aHeight);

  mCallback = aCallback;

  mDecoderLooper->registerHandler(this);
  mDecoderLooper->start();
  mCodecLooper->start();

  mCodec = MediaCodec::CreateByType(mCodecLooper, aMime, false);
  if (!mCodec) {
    return UNKNOWN_ERROR;
  }

  sp<Surface> surface = InitBufferQueue();
  sp<AMessage> config = new AMessage();
  config->setString("mime", aMime);
  config->setInt32("width", aWidth);
  config->setInt32("height", aHeight);

  status_t err = mCodec->configure(config, surface, nullptr, 0);
  if (err != OK) {
    LOGE("Decoder:%p failed to configure codec", this);
    return err;
  }

  // Run MediaCodec in async mode.
  sp<AMessage> reply = new AMessage(kWhatCodecNotify, this);
  err = mCodec->setCallback(reply);
  if (err != OK) {
    LOGE("Decoder:%p failed to set codec callback", this);
    return err;
  }

  err = mCodec->start();
  if (err != OK) {
    LOGE("Decoder:%p failed to start codec", this);
    return err;
  }
  return OK;
}

status_t WebrtcGonkVideoDecoder::Release() {
  LOGD("Decoder:%p releasing", this);

  mDecoderLooper->stop();
  mDecoderLooper->unregisterHandler(id());

  if (mCodec) {
    mCodec->release();
    mCodec = nullptr;
  }
  if (mNativeWindow) {
    mNativeWindow->abandon();
    mNativeWindow = nullptr;
  }
  mInputFrames.clear();
  mInputBuffers.clear();
  mDecodedFrameInfo = nullptr;
  mCallback = nullptr;
  return OK;
}

sp<Surface> WebrtcGonkVideoDecoder::InitBufferQueue() {
  // Extend Surface class and override its queueBuffer() in order to let the
  // decoder know which timestamp is bound to the graphic buffer we just queued.
  class SurfaceExt : public Surface {
   public:
    explicit SurfaceExt(const wp<WebrtcGonkVideoDecoder>& aDecoder,
                        const sp<IGraphicBufferProducer>& aBufferProducer)
        : Surface(aBufferProducer, false), mDecoder(aDecoder) {}

   private:
    virtual int queueBuffer(ANativeWindowBuffer* aBuffer,
                            int aFenceFd) override {
      if (auto decoder = mDecoder.promote()) {
        decoder->OnOutputBufferQueued(aBuffer, mTimestamp);
      }
      return Surface::queueBuffer(aBuffer, aFenceFd);
    }

    wp<WebrtcGonkVideoDecoder> mDecoder;
  };

  sp<IGraphicBufferProducer> producer;
  sp<IGonkGraphicBufferConsumer> consumer;
  GonkBufferQueue::createBufferQueue(&producer, &consumer);

  mNativeWindow = new GonkNativeWindow(consumer);
  mNativeWindow->setNewFrameCallback(this);

  static_cast<GonkBufferQueueProducer*>(producer.get())
      ->setSynchronousMode(false);
  // More spare buffers to avoid MediaCodec waiting for native window
  consumer->setMaxAcquiredBufferCount(WEBRTC_OMX_MIN_DECODE_BUFFERS);
  return new SurfaceExt(this, producer);
}

status_t WebrtcGonkVideoDecoder::Decode(const webrtc::EncodedImage& aEncoded,
                                        bool aIsCodecConfig,
                                        int64_t aRenderTimeMs) {
  if (!mCodec || !aEncoded.data() || !aEncoded.size()) {
    return INVALID_OPERATION;
  }

  sp<FrameInfo> frameInfo = new FrameInfo();
  frameInfo->mWidth = aEncoded._encodedWidth;
  frameInfo->mHeight = aEncoded._encodedHeight;
  frameInfo->mTimestamp = aEncoded.Timestamp();
  // Unwrap RTP timestamp and convert it from 90kHz clock to micro seconds.
  frameInfo->mTimestampUs = mUnwrapper.Unwrap(aEncoded.Timestamp()) * 1000 / 90;
  frameInfo->mRenderTimeMs = aRenderTimeMs;
  frameInfo->mIsCodecConfig = aIsCodecConfig;
  frameInfo->mInputTimeNs = systemTime();

  // We cannot take ownership of |aEncoded|, so make a copy of it.
  sp<ABuffer> frameData =
      ABuffer::CreateAsCopy(aEncoded.data(), aEncoded.size());

  sp<AMessage> msg = new AMessage(kWhatQueueInputData, this);
  msg->setObject("frame-info", frameInfo.get());
  msg->setObject("frame-data", frameData.get());
  msg->post();
  return OK;
}

void WebrtcGonkVideoDecoder::onMessageReceived(const sp<AMessage>& aMsg) {
  if (IsVerboseLoggingEnabled()) {
    auto lines = SplitMultilineString(aMsg->debugString().c_str());
    for (auto& line : lines) {
      LOGV("Decoder:%p onMessage: %s", this, line.c_str());
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
          OnFillInputBuffers();
          break;
        }

        case MediaCodec::CB_OUTPUT_AVAILABLE: {
          int32_t index;
          int64_t timeUs;
          CHECK(aMsg->findInt32("index", &index));
          CHECK(aMsg->findInt64("timeUs", &timeUs));
          mCodec->renderOutputBufferAndRelease(index);
          break;
        }

        case MediaCodec::CB_OUTPUT_FORMAT_CHANGED: {
          sp<AMessage> format;
          CHECK(aMsg->findMessage("format", &format));
          auto lines = SplitMultilineString(format->debugString().c_str());
          for (auto& line : lines) {
            LOGI("Decoder:%p format changed: %s", this, line.c_str());
          }
          break;
        }

        case MediaCodec::CB_ERROR: {
          status_t err;
          CHECK(aMsg->findInt32("err", &err));
          LOGE("Decoder:%p reported error: 0x%x", this, err);
          break;
        }

        default: {
          TRESPASS();
          break;
        }
      }

      break;
    }

    case kWhatQueueInputData: {
      sp<RefBase> obj;
      CHECK(aMsg->findObject("frame-info", &obj));
      sp<FrameInfo> frameInfo = static_cast<FrameInfo*>(obj.get());
      CHECK(aMsg->findObject("frame-data", &obj));
      sp<ABuffer> frameData = static_cast<ABuffer*>(obj.get());

      mInputFrames.push_back(std::make_pair(frameInfo, frameData));
      OnFillInputBuffers();
      break;
    }
  }
}

void WebrtcGonkVideoDecoder::OnFillInputBuffers() {
  while (mInputFrames.size() && mInputBuffers.size()) {
    auto index = mInputBuffers.front();
    mInputBuffers.pop_front();

    sp<MediaCodecBuffer> buffer;
    mCodec->getInputBuffer(index, &buffer);
    if (!buffer) {
      LOGE("Decoder:%p failed to get input buffer", this);
      continue;
    }

    auto [frameInfo, frameData] = mInputFrames.front();
    mInputFrames.pop_front();

    if (buffer->capacity() < frameData->size()) {
      LOGE("Decoder:%p exceed input buffer size, dropping one frame", this);
      mInputBuffers.push_front(index);  // keep the buffer
      continue;
    }

    buffer->setRange(0, frameData->size());
    memcpy(buffer->data(), frameData->data(), frameData->size());

    uint32_t flags =
        frameInfo->mIsCodecConfig ? MediaCodec::BUFFER_FLAG_CODECCONFIG : 0;

    status_t err = mCodec->queueInputBuffer(index, 0, frameData->size(),
                                            frameInfo->mTimestampUs, flags);
    if (err != OK) {
      LOGE("Decoder:%p failed to queue input buffer", this);
      continue;
    }

    if (!frameInfo->mIsCodecConfig) {
      mFrameInfoQueue.Push(frameInfo);
    }
  }
}

void WebrtcGonkVideoDecoder::OnOutputBufferQueued(ANativeWindowBuffer* aBuffer,
                                                  int64_t aTimestampNs) {
  mDecodedFrameInfo = mFrameInfoQueue.Pop(aTimestampNs / 1000);
  mDecodedFrameInfo->mNativeBuffer = aBuffer;
}

void WebrtcGonkVideoDecoder::OnNewFrame() {
  using mozilla::ImageBuffer;
  using mozilla::MakeRefPtr;
  using mozilla::layers::GrallocImage;
  using mozilla::layers::TextureClient;

  RefPtr<TextureClient> buffer = mNativeWindow->getCurrentBuffer();
  if (!buffer) {
    LOGE("Decoder:%p failed to get texture client", this);
    return;
  }

  if (!mDecodedFrameInfo) {
    LOGE("Decoder:%p no decoded frame info", this);
    return;
  }

  auto image = MakeRefPtr<GrallocImage>();
  image->AdoptData(buffer, buffer->GetSize());

  if (image->GetGraphicBuffer().get() != mDecodedFrameInfo->mNativeBuffer) {
    LOGE("Decoder:%p graphic buffer mismatch", this);
  }

  int64_t latency = (systemTime() - mDecodedFrameInfo->mInputTimeNs) / 1000000;
  LOGD("Decoder:%p frame decoded, latency %" PRId64 " ms", this, latency);

  rtc::scoped_refptr<ImageBuffer> imageBuffer =
      rtc::make_ref_counted<ImageBuffer>(std::move(image));
  webrtc::VideoFrame videoFrame(imageBuffer, mDecodedFrameInfo->mTimestamp,
                                mDecodedFrameInfo->mRenderTimeMs,
                                webrtc::kVideoRotation_0);

  CHECK(mCallback);
  mCallback->OnDecoded(videoFrame);
}

}  // namespace android
