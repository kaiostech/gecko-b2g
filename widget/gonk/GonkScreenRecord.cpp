/*
 * Copyright 2013 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <binder/IPCThreadState.h>
#include <utils/Trace.h>

#include <gui/IProducerListener.h>
#include <gui/Surface.h>
#include <media/openmax/OMX_IVCommon.h>
#include <media/stagefright/foundation/ABuffer.h>
#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/MediaCodec.h>
#include <media/stagefright/MediaCodecConstants.h>
#include <media/stagefright/MediaErrors.h>
#include <media/stagefright/MediaMuxer.h>
#if ANDROID_VERSION == 30
#include <mediadrm/ICrypto.h>
#else
#include <media/ICrypto.h>
#endif
#include <media/MediaCodecBuffer.h>

#include "GfxDebugger_defs.h"
#include "GonkScreenRecord.h"
#include "libdisplay/GonkDisplay.h"
#include "ScreenHelperGonk.h"

using namespace android;

#ifdef LOG_TAG
#  undef LOG_TAG
#endif
#define LOG_TAG "GonkScreenRecord"

#include <android/log.h>
#define GS_LOGD(args...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, args)
#define GS_LOGI(args...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, args)
#define GS_LOGW(args...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, args)
#define GS_LOGE(args...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, args)

namespace mozilla {

__attribute__((visibility("default"))) void HookSetVsyncAlwaysEnabled(
    bool aAlways);

__attribute__((visibility("default"))) void HookEnableVsync(bool aAlways);

#define GS_NUM_BUFFER_SLOTS (64)
#define GS_USAGE_CPU_WRITE_OFTEN (3 << 4)

static const uint32_t kFallbackWidth = 1280;  // 720p
static const uint32_t kFallbackHeight = 720;
static const char* kMimeTypeAvc = "video/avc";

// Set by signal handler to stop recording.
static volatile bool gStopRequested = false;
static sp<IGraphicBufferProducer> gEncoderBufferProducer = nullptr;

/*
 * Configures and starts the MediaCodec encoder.  Obtains an input surface
 * from the codec.
 */
static status_t prepareEncoder(uint32_t videoWidth, uint32_t videoHeight,
                               float displayFps, sp<MediaCodec>* pCodec,
                               sp<IGraphicBufferProducer>* pBufferProducer) {
  status_t err;
  uint32_t bitRate = 20000000;  // default 20Mbps
  uint32_t bFrames = 0;

  GS_LOGI("Configuring recorder for %dx%d %s at %.2fMbps\n", videoWidth,
          videoHeight, kMimeTypeAvc, bitRate / 1000000.0);

  sp<AMessage> format = new AMessage;
  format->setInt32(KEY_WIDTH, videoWidth);
  format->setInt32(KEY_HEIGHT, videoHeight);
  format->setString(KEY_MIME, kMimeTypeAvc);
  format->setInt32(KEY_COLOR_FORMAT, OMX_COLOR_FormatAndroidOpaque);
  format->setInt32(KEY_BIT_RATE, bitRate);
  format->setFloat(KEY_FRAME_RATE, displayFps);
  format->setInt32(KEY_I_FRAME_INTERVAL, 10);
  format->setInt32(KEY_MAX_B_FRAMES, bFrames);
  if (bFrames > 0) {
    format->setInt32(KEY_PROFILE, AVCProfileMain);
    format->setInt32(KEY_LEVEL, AVCLevel41);
  }

  sp<android::ALooper> looper = new android::ALooper;
  looper->setName("screenrecord_looper");
  looper->start();
  GS_LOGD("Creating codec");
  sp<MediaCodec> codec;
  codec = MediaCodec::CreateByType(looper, kMimeTypeAvc, true);
  if (codec == NULL) {
    GS_LOGE("ERROR: unable to create %s codec instance\n", kMimeTypeAvc);
    return UNKNOWN_ERROR;
  }

  err = codec->configure(format, NULL, NULL, MediaCodec::CONFIGURE_FLAG_ENCODE);
  if (err != NO_ERROR) {
    GS_LOGE("ERROR: unable to configure %s codec at %dx%d (err=%d)\n",
            kMimeTypeAvc, videoWidth, videoHeight, err);
    codec->release();
    return err;
  }

  GS_LOGD("Creating encoder input surface");
  sp<IGraphicBufferProducer> bufferProducer;
  err = codec->createInputSurface(&bufferProducer);
  if (err != NO_ERROR) {
    GS_LOGE("ERROR: unable to create encoder input surface (err=%d)\n", err);
    codec->release();
    return err;
  }

  GS_LOGD("Starting codec");
  err = codec->start();
  if (err != NO_ERROR) {
    GS_LOGE("ERROR: unable to start codec (err=%d)\n", err);
    codec->release();
    return err;
  }

  GS_LOGD("Codec prepared");
  *pCodec = codec;
  *pBufferProducer = bufferProducer;
  return NO_ERROR;
}

#define GONK_PIXEL_FORMAT_RGBA_8888 1
#define GONK_PIXEL_FORMAT_RGB_565 4
#define RGB565_FORMAT_BPP 2
#define NV12_FORMAT_BPP 1

#define IS_ODD(x) !(x & 0x1)

static status_t RGB565ToNV12(const uint8_t* src_rgb565, int src_stride_rgb565,
                             uint8_t* dst_y, int dst_stride_y, uint8_t* dst_uv,
                             int dst_stride_uv, int width, int height) {
  if (width > src_stride_rgb565 || width > dst_stride_y) {
    GS_LOGE("Error: stride < width src(%d < %d) dst (%d < %d)",
            src_stride_rgb565, width, dst_stride_y, width);
    return INVALID_OPERATION;
  }
  int srcStride = src_stride_rgb565 * RGB565_FORMAT_BPP;
  int dstStride = dst_stride_y * NV12_FORMAT_BPP;
  for (int linePos = 0; linePos < height; linePos++) {
    for (int bytePos = 0; bytePos < (width * RGB565_FORMAT_BPP); bytePos += 2) {
      int pixelPos = bytePos / 2;
      uint8_t R = (src_rgb565[bytePos + 1] & 0xF8);
      uint8_t G = ((src_rgb565[bytePos] & 0xE0) >> 3) |
                  ((src_rgb565[bytePos + 1] & 0x07) << 5);
      uint8_t B = ((src_rgb565[bytePos] & 0x1F) << 3);
      float Y = 0.257 * R + 0.504 * G + 0.098 * B + 16;
      dst_y[pixelPos] = (uint8_t)Y;

      // Store uv element for odd lines and pixels.
      if (IS_ODD(linePos) && IS_ODD(pixelPos)) {
        float U = -0.148 * R - 0.291 * G + 0.439 * B + 128;
        float V = 0.439 * R - 0.368 * G - 0.071 * B + 128;
        dst_uv[pixelPos] = (uint8_t)U;
        dst_uv[pixelPos + 1] = (uint8_t)V;
      }
    }

    src_rgb565 += srcStride;
    dst_y += dstStride;
    // Increment uv stride only for odd lines.
    if (IS_ODD(linePos)) {
      dst_uv += dstStride;
    }
  }

  return NO_ERROR;
}

// This function will be invoked once vsync period expired. The main task will
// be:
// 1. Fetch display buffer from GonkDisplay.
// 2. Convert the data from RGB to NV12.
// 3. Push the prepared buffer into bufferQueue for encoder.
void GonkScreenRecord::NotifyVsync(const int display,
                                   const TimeStamp& aVsyncTimestamp,
                                   const TimeDuration& aVsyncPeriod) {
  if (gEncoderBufferProducer.get()) {
    void* srcBuffer;
    sp<GraphicBuffer> frameBuffer =
        GetGonkDisplay()->GetFrameBuffer((DisplayType)display);
    if (frameBuffer->lock(GraphicBuffer::USAGE_SW_READ_OFTEN, &srcBuffer) !=
        OK) {
      GS_LOGE("Error: lock srcBuffer failed!!");
      return;
    }

    int dequeuedSlot = -1;
    sp<Fence> dequeuedFence;
    sp<GraphicBuffer> dequeuedBuffer;
    android_ycbcr ycbcr;
    IGraphicBufferProducer::QueueBufferInput queueBufferInput(
        (aVsyncTimestamp - TimeStamp()).ToMicroseconds() * 1000, false,
        HAL_DATASPACE_UNKNOWN,
        Rect(frameBuffer->getWidth(), frameBuffer->getHeight()), 0, 0,
        Fence::NO_FENCE);
    IGraphicBufferProducer::QueueBufferOutput queueBufferOutput;

    gEncoderBufferProducer->dequeueBuffer(
        &dequeuedSlot, &dequeuedFence, frameBuffer->getWidth(),
        frameBuffer->getHeight(), 0, (uint64_t)GS_USAGE_CPU_WRITE_OFTEN,
        nullptr, nullptr);

    if (dequeuedSlot < 0 || dequeuedSlot > GS_NUM_BUFFER_SLOTS) {
      GS_LOGE("Error: dequeuedSlot is invalid!!");
      goto unlockSrcBuffer;
    }

    gEncoderBufferProducer->requestBuffer(dequeuedSlot, &dequeuedBuffer);
    if (dequeuedBuffer->lockYCbCr(GraphicBuffer::USAGE_SW_WRITE_OFTEN,
                                  &ycbcr) != OK) {
      GS_LOGE("Error: lock dstBuffer failed!!");
      goto unlockSrcBuffer;
    }

    if (RGB565ToNV12((uint8_t*)srcBuffer, frameBuffer->getStride(),
                     (uint8_t*)ycbcr.y, ycbcr.ystride, (uint8_t*)ycbcr.cb,
                     ycbcr.cstride, frameBuffer->getWidth(),
                     frameBuffer->getHeight()) != OK) {
      GS_LOGE("Error: buffer conversion for RGB565ToNV12 failed!!");
      goto unlockAllBuffer;
    }

    gEncoderBufferProducer->queueBuffer(dequeuedSlot, queueBufferInput,
                                        &queueBufferOutput);

  unlockAllBuffer:
    dequeuedBuffer->unlock();
  unlockSrcBuffer:
    frameBuffer->unlock();
  }

  // Schedule next vsync if recording has not yet been stopped.
  if (!gStopRequested) {
    TimeStamp nextVsync = aVsyncTimestamp + aVsyncPeriod;
    TimeDuration delay = nextVsync - TimeStamp::Now();
    if (delay.ToMilliseconds() < 0) {
      delay = TimeDuration::FromMilliseconds(0);
      nextVsync = TimeStamp::Now();
    }

    mCurrentVsyncTask =
        NewCancelableRunnableMethod<int, TimeStamp, TimeDuration>(
            "GonkScreenRecord::NotifyVsync", this,
            &GonkScreenRecord::NotifyVsync, display, nextVsync, aVsyncPeriod);

    RefPtr<Runnable> addrefedTask = mCurrentVsyncTask;
    mVsyncThread->message_loop()->PostDelayedTask(addrefedTask.forget(),
                                                  delay.ToMilliseconds());
  }
}

void GonkScreenRecord::ClearVsyncTask() {
  if (mCurrentVsyncTask) {
    mCurrentVsyncTask->Cancel();
    mCurrentVsyncTask = nullptr;
  }
}

/*
 * Runs the MediaCodec encoder, sending the output to the MediaMuxer.  The
 * input frames are coming from the virtual display as fast as SurfaceFlinger
 * wants to send them.
 *
 * Exactly one of muxer or rawFp must be non-null.
 *
 * The muxer must *not* have been started before calling.
 */
static status_t runEncoder(const sp<MediaCodec>& encoder,
                           const sp<MediaMuxer>& muxer, FILE* rawFp,
                           const RefPtr<nsScreenGonk>& primaryScreen,
                           uint32_t rotation, uint32_t timeLimitSec) {
  static int kTimeout = 250000;  // be responsive on signal
  status_t err;
  ssize_t trackIdx = -1;
  uint32_t debugNumFrames = 0;
  int64_t startWhenNsec = systemTime(CLOCK_MONOTONIC);
  int64_t endWhenNsec = startWhenNsec + seconds_to_nanoseconds(timeLimitSec);

  assert((rawFp == NULL && muxer != NULL) || (rawFp != NULL && muxer == NULL));

  android::Vector<sp<MediaCodecBuffer> > buffers;
  err = encoder->getOutputBuffers(&buffers);
  if (err != NO_ERROR) {
    GS_LOGE("Unable to get output buffers (err=%d)\n", err);
    return err;
  }

  // Run until we're signaled.
  while (!gStopRequested) {
    size_t bufIndex, offset, size;
    int64_t ptsUsec;
    uint32_t flags;

    if (systemTime(CLOCK_MONOTONIC) > endWhenNsec) {
      GS_LOGI("Time limit reached\n");
      gStopRequested = true;
      break;
    }

    err = encoder->dequeueOutputBuffer(&bufIndex, &offset, &size, &ptsUsec,
                                       &flags, kTimeout);
    switch (err) {
      case NO_ERROR:
        // got a buffer
        if ((flags & MediaCodec::BUFFER_FLAG_CODECCONFIG) != 0) {
          GS_LOGD("Got codec config buffer (%zu bytes)", size);
          if (muxer != NULL) {
            // ignore this -- we passed the CSD into MediaMuxer when
            // we got the format change notification
            size = 0;
          }
        }
        if (size != 0) {
          // If the virtual display isn't providing us with timestamps,
          // use the current time.  This isn't great -- we could get
          // decoded data in clusters -- but we're not expecting
          // to hit this anyway.
          if (ptsUsec == 0) {
            ptsUsec = systemTime(SYSTEM_TIME_MONOTONIC) / 1000;
          }

          if (muxer == NULL) {
            fwrite(buffers[bufIndex]->data(), 1, size, rawFp);
            // Flush the data immediately in case we're streaming.
            // We don't want to do this if all we've written is
            // the SPS/PPS data because mplayer gets confused.
            if ((flags & MediaCodec::BUFFER_FLAG_CODECCONFIG) == 0) {
              fflush(rawFp);
            }
          } else {
            // The MediaMuxer docs are unclear, but it appears that we
            // need to pass either the full set of BufferInfo flags, or
            // (flags & BUFFER_FLAG_SYNCFRAME).
            //
            // If this blocks for too long we could drop frames.  We may
            // want to queue these up and do them on a different thread.
            ATRACE_NAME("write sample");
            assert(trackIdx != -1);
            // TODO
            sp<ABuffer> buffer = new ABuffer(buffers[bufIndex]->data(),
                                             buffers[bufIndex]->size());
            err = muxer->writeSampleData(buffer, trackIdx, ptsUsec, flags);
            if (err != NO_ERROR) {
              GS_LOGE("Failed writing data to muxer (err=%d)\n", err);
              return err;
            }
          }
          debugNumFrames++;
        }
        err = encoder->releaseOutputBuffer(bufIndex);
        if (err != NO_ERROR) {
          GS_LOGE("Unable to release output buffer (err=%d)\n", err);
          return err;
        }
        if ((flags & MediaCodec::BUFFER_FLAG_EOS) != 0) {
          // Not expecting EOS from SurfaceFlinger.  Go with it.
          GS_LOGD("Received end-of-stream");
          gStopRequested = true;
        }
        break;
      case -EAGAIN:  // INFO_TRY_AGAIN_LATER
        GS_LOGD("Got -EAGAIN, looping");
        break;
      case android::INFO_FORMAT_CHANGED:  // INFO_OUTPUT_FORMAT_CHANGED
      {
        // Format includes CSD, which we must provide to muxer.
        GS_LOGD("Encoder format changed");
        sp<AMessage> newFormat;
        encoder->getOutputFormat(&newFormat);
        if (muxer != NULL) {
          trackIdx = muxer->addTrack(newFormat);
          GS_LOGD("Starting muxer");
          err = muxer->start();
          if (err != NO_ERROR) {
            GS_LOGE("Unable to start muxer (err=%d)\n", err);
            return err;
          }
        }
      } break;
      case android::INFO_OUTPUT_BUFFERS_CHANGED:  // INFO_OUTPUT_BUFFERS_CHANGED
        // Not expected for an encoder; handle it anyway.
        GS_LOGD("Encoder buffers changed");
        err = encoder->getOutputBuffers(&buffers);
        if (err != NO_ERROR) {
          GS_LOGE("Unable to get new output buffers (err=%d)\n", err);
          return err;
        }
        break;
      case INVALID_OPERATION:
        GS_LOGW("dequeueOutputBuffer returned INVALID_OPERATION");
        return err;
      default:
        GS_LOGE("Got weird result %d from dequeueOutputBuffer\n", err);
        return err;
    }
  }

  GS_LOGD("Encoder stopping (req=%d)", gStopRequested);
  GS_LOGI("Encoder stopping; recorded %u frames in %" PRId64 " seconds\n",
          debugNumFrames,
          nanoseconds_to_seconds(systemTime(CLOCK_MONOTONIC) - startWhenNsec));
  return NO_ERROR;
}

static inline uint32_t floorToEven(uint32_t num) { return num & ~1; }

/*
 * Main "do work" start point.
 *
 * Configures codec, muxer, and virtual display, then starts moving bits
 * around.
 */
static status_t recordScreen(GonkScreenRecord* screenRecordInst,
                             uint32_t displayId, uint32_t outputFormat,
                             uint32_t timeLimitSec, const char* fileName) {
  status_t err;

  // Start Binder thread pool.  MediaCodec needs to be able to receive
  // messages from mediaserver.
  sp<ProcessState> self = ProcessState::self();
  self->startThreadPool();

  // Get main display parameters.
  uint32_t displayWidth, displayHeight, rotation;
  uint64_t vsyncPeriod;
  float fps;
  RefPtr<nsScreenGonk> screenGonk =
      widget::ScreenHelperGonk::GetScreenGonk(displayId);
  if (screenGonk.get()) {
    displayWidth = screenGonk->GetNaturalBounds().Width();
    displayHeight = screenGonk->GetNaturalBounds().Height();
    screenGonk->GetRotation(&rotation);
    // Currently only support RGB565 display buffer format.
    if (GONK_PIXEL_FORMAT_RGB_565 != screenGonk->GetSurfaceFormat()) {
      GS_LOGE("ERROR: unsupported display format 0x%x\n",
              screenGonk->GetSurfaceFormat());
      return INVALID_OPERATION;
    }
    // Vsync period can be obtained through primary display.
    vsyncPeriod = GetGonkDisplay()
                      ->GetDispNativeData(DisplayType::DISPLAY_PRIMARY)
                      .mVsyncPeriod;
    fps = 1000000000.0 / (float)vsyncPeriod;
  } else {
    GS_LOGD("ERROR: unable to get primary screen\n");
    return NO_INIT;
  }

  GS_LOGI("Main display is %dx%d @%.2ffps (orientation=%u)\n", displayWidth,
          displayHeight, fps, rotation);

  // Encoder can't take odd number as config
  uint32_t videoWidth, videoHeight;
  videoWidth = floorToEven(displayWidth);
  videoHeight = floorToEven(displayHeight);

  // Configure and start the encoder.
  sp<MediaCodec> encoder;
  err = prepareEncoder(videoWidth, videoHeight, fps, &encoder,
                       &gEncoderBufferProducer);

  if (err != NO_ERROR) {
    // fallback is defined for landscape; swap if we're in portrait
    bool needSwap = videoWidth < videoHeight;
    uint32_t newWidth = needSwap ? kFallbackHeight : kFallbackWidth;
    uint32_t newHeight = needSwap ? kFallbackWidth : kFallbackHeight;
    if (videoWidth != newWidth && videoHeight != newHeight) {
      GS_LOGD("Retrying with 720p");
      GS_LOGE("WARNING: failed at %dx%d, retrying at %dx%d\n", videoWidth,
              videoHeight, newWidth, newHeight);
      videoWidth = newWidth;
      videoHeight = newHeight;
      err = prepareEncoder(videoWidth, videoHeight, fps, &encoder,
                           &gEncoderBufferProducer);
    }
  }
  if (err != NO_ERROR) return err;

  // From here on, we must explicitly release() the encoder before it goes
  // out of scope, or we will get an assertion failure from stagefright
  // later on in a different thread.

  sp<MediaMuxer> muxer = NULL;
  FILE* rawFp = NULL;
  IGraphicBufferProducer::QueueBufferOutput output;
  TimeStamp vsyncTime;
  TimeDuration vsyncPeriodDuration;
  switch (outputFormat) {
    case FORMAT_MP4:
    case FORMAT_WEBM:
    case FORMAT_3GPP: {
      // Configure muxer.  We have to wait for the CSD blob from the encoder
      // before we can start it.
      err = unlink(fileName);
      if (err != 0 && errno != ENOENT) {
        GS_LOGE("ERROR: couldn't remove existing file\n");
        goto CleanUp;
      }
      int fd = open(fileName, O_CREAT | O_LARGEFILE | O_TRUNC | O_RDWR,
                    S_IRUSR | S_IWUSR);
      if (fd < 0) {
        GS_LOGE("ERROR: couldn't open file\n");
        goto CleanUp;
      }
      if (outputFormat == FORMAT_MP4) {
        muxer = new MediaMuxer(fd, MediaMuxer::OUTPUT_FORMAT_MPEG_4);
      } else if (outputFormat == FORMAT_WEBM) {
        muxer = new MediaMuxer(fd, MediaMuxer::OUTPUT_FORMAT_WEBM);
      } else {
        muxer = new MediaMuxer(fd, MediaMuxer::OUTPUT_FORMAT_THREE_GPP);
      }
      close(fd);
      // set screen rotation to meta for player to do rotation
      muxer->setOrientationHint(rotation);  // TODO: does this do anything?
      break;
    }
    default:
      GS_LOGE("ERROR: unknown format %d\n", outputFormat);
      goto CleanUp;
  }

  if (OK != gEncoderBufferProducer->connect(nullptr, NATIVE_WINDOW_API_CPU,
                                            false, &output)) {
    GS_LOGE("ERROR: failed to connect bufferProducer!\n");
    goto CleanUp;
  }

  vsyncTime = TimeStamp::Now();
  vsyncPeriodDuration =
      mozilla::TimeDuration::FromMicroseconds(vsyncPeriod / 1000);
  // Kick SW Vsync to start screen recording.
  screenRecordInst->NotifyVsync(displayId, vsyncTime, vsyncPeriodDuration);

  // Main encoder loop.
  err = runEncoder(encoder, muxer, rawFp, screenGonk, rotation, timeLimitSec);
  if (err != NO_ERROR) {
    GS_LOGE("Encoder failed (err=%d)\n", err);
    // fall through to cleanup
  }

  GS_LOGD("Stopping encoder and muxer\n");

CleanUp:
  // Shut everything down, starting with the producer side.
  gEncoderBufferProducer = NULL;
  if (encoder != NULL) encoder->stop();
  if (muxer != NULL) {
    // If we don't stop muxer explicitly, i.e. let the destructor run,
    // it may hang (b/11050628).
    err = muxer->stop();
  } else if (rawFp && rawFp != stdout) {
    fclose(rawFp);
  }
  if (encoder != NULL) encoder->release();

  return err;
}

int GonkScreenRecord::capture() {
  if (mOutputFormat == FORMAT_MP4) {
    // MediaMuxer tries to create the file in the constructor, but we don't
    // learn about the failure until muxer.start(), which returns a generic
    // error code without logging anything.  We attempt to create the file
    // now for better diagnostics.
    int fd = open(mFileName, O_CREAT | O_RDWR, 0644);
    if (fd < 0) {
      GS_LOGE("Unable to open '%s': %s\n", mFileName, strerror(errno));
      if (mFinishCallback) {
        mFinishCallback(mStreamFd, errno);
      }
      return errno;
    }
    close(fd);
  }

  gStopRequested = false;
  mVsyncThread = new ::base::Thread("SoftwareVsyncThread");
  MOZ_RELEASE_ASSERT(mVsyncThread->Start(),
                     "GFX: Could not start software vsync thread");

  status_t err =
      recordScreen(this, mDisplayId, mOutputFormat, mTimeLimitSec, mFileName);
  GS_LOGI(err == NO_ERROR ? "success" : "failed");

  if (mFinishCallback) {
    mFinishCallback(mStreamFd, err);
  }

  mVsyncThread->message_loop()->PostTask(
      NewRunnableMethod("GonkScreenRecord::ClearVsyncTask", this,
                        &GonkScreenRecord::ClearVsyncTask));

  // Wait for SW Vsync thread to finish and quit.
  mVsyncThread->Stop();
  delete mVsyncThread;

  return err;
}

} /* namespace mozilla */
