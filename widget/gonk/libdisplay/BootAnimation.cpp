/* Copyright 2012 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <algorithm>
#include <endian.h>
#include <fcntl.h>
#include <pthread.h>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <vector>
#include "mozilla/Sprintf.h"
#include "png.h"

#include "android/log.h"
#include "hardware/gralloc.h"
#include "cutils/properties.h"

#include "NativeGralloc.h"
#include <dlfcn.h>

#include "GonkDisplay.h"

#define LOG(args...) __android_log_print(ANDROID_LOG_INFO, "Gonk", ##args)
#define LOGW(args...) __android_log_print(ANDROID_LOG_WARN, "Gonk", ##args)
#define LOGE(args...) __android_log_print(ANDROID_LOG_ERROR, "Gonk", ##args)

#define PRIMARY_SCREEN_BRIGHTNESS "/sys/class/leds/lcd-backlight/brightness"
#define SECONDARY_SCREEN_BRIGHTNESS \
  "/sys/class/leds/sublcd-backlight/brightness"
// Constants defined in system_properties.h from AOSP
#define VALUE_MAX_LENGTH 92

using namespace mozilla;
using namespace std;

namespace mozilla {
__attribute__((visibility("default"))) void HookSetVsyncAlwaysEnabled(
    bool aAlways);
}

static pthread_t sAnimationThread;
static bool sRunAnimation;

/* See http://www.pkware.com/documents/casestudies/APPNOTE.TXT */
struct local_file_header {
  uint32_t signature;
  uint16_t min_version;
  uint16_t general_flag;
  uint16_t compression;
  uint16_t lastmod_time;
  uint16_t lastmod_date;
  uint32_t crc32;
  uint32_t compressed_size;
  uint32_t uncompressed_size;
  uint16_t filename_size;
  uint16_t extra_field_size;
  char data[0];

  uint32_t GetDataSize() const { return letoh32(uncompressed_size); }

  uint32_t GetSize() const {
    /* XXX account for data descriptor */
    return sizeof(local_file_header) + letoh16(filename_size) +
           letoh16(extra_field_size) + GetDataSize();
  }

  const char* GetData() const {
    return data + letoh16(filename_size) + letoh16(extra_field_size);
  }
} __attribute__((__packed__));

struct data_descriptor {
  uint32_t crc32;
  uint32_t compressed_size;
  uint32_t uncompressed_size;
} __attribute__((__packed__));

struct cdir_entry {
  uint32_t signature;
  uint16_t creator_version;
  uint16_t min_version;
  uint16_t general_flag;
  uint16_t compression;
  uint16_t lastmod_time;
  uint16_t lastmod_date;
  uint32_t crc32;
  uint32_t compressed_size;
  uint32_t uncompressed_size;
  uint16_t filename_size;
  uint16_t extra_field_size;
  uint16_t file_comment_size;
  uint16_t disk_num;
  uint16_t internal_attr;
  uint32_t external_attr;
  uint32_t offset;
  char data[0];

  uint32_t GetDataSize() const { return letoh32(compressed_size); }

  uint32_t GetSize() const {
    return sizeof(cdir_entry) + letoh16(filename_size) +
           letoh16(extra_field_size) + letoh16(file_comment_size);
  }

  bool Valid() const { return signature == htole32(0x02014b50); }
} __attribute__((__packed__));

struct cdir_end {
  uint32_t signature;
  uint16_t disk_num;
  uint16_t cdir_disk;
  uint16_t disk_entries;
  uint16_t cdir_entries;
  uint32_t cdir_size;
  uint32_t cdir_offset;
  uint16_t comment_size;
  char comment[0];

  bool Valid() const { return signature == htole32(0x06054b50); }
} __attribute__((__packed__));

/* We don't have access to libjar and the zip reader in android
 * doesn't quite fit what we want to do. */
class ZipReader {
  const char* mBuf;
  const cdir_end* mEnd;
  const char* mCdir_limit;
  uint32_t mBuflen;

 public:
  ZipReader() : mBuf(nullptr) {}
  ~ZipReader() {
    if (mBuf) munmap((void*)mBuf, mBuflen);
  }

  // Take over the ownership of mBuf when copying.
  // This is needed because we use vector to store Animation
  ZipReader(const ZipReader& aOther) {
    ZipReader& other = const_cast<ZipReader&>(aOther);
    memcpy(this, &other, sizeof(ZipReader));
    other.mBuf = nullptr;
  }

  ZipReader& operator=(const ZipReader& aOther) {
    if (mBuf) munmap((void*)mBuf, mBuflen);

    ZipReader& other = const_cast<ZipReader&>(aOther);
    memcpy(this, &other, sizeof(ZipReader));
    other.mBuf = nullptr;

    return *this;
  }

  bool OpenArchive(const char* path) {
    int fd;
    do {
      fd = open(path, O_RDONLY);
    } while (fd == -1 && errno == EINTR);
    if (fd == -1) return false;

    struct stat sb;
    if (fstat(fd, &sb) == -1 || sb.st_size < sizeof(cdir_end)) {
      close(fd);
      return false;
    }

    mBuflen = sb.st_size;
    mBuf = (char*)mmap(nullptr, sb.st_size, PROT_READ, MAP_SHARED, fd, 0);
    close(fd);

    if (!mBuf) {
      return false;
    }

    madvise(const_cast<void*>(static_cast<const void*>(mBuf)), sb.st_size,
            MADV_SEQUENTIAL);

    mEnd = (cdir_end*)(mBuf + mBuflen - sizeof(cdir_end));
    while (!mEnd->Valid() && (char*)mEnd > mBuf) {
      mEnd = (cdir_end*)((char*)mEnd - 1);
    }

    mCdir_limit = mBuf + letoh32(mEnd->cdir_offset) + letoh32(mEnd->cdir_size);

    if (!mEnd->Valid() || mCdir_limit > (char*)mEnd) {
      munmap((void*)mBuf, mBuflen);
      mBuf = nullptr;
      return false;
    }

    return true;
  }

  /* Pass null to get the first cdir entry */
  const cdir_entry* GetNextEntry(const cdir_entry* prev) {
    const cdir_entry* entry;
    if (prev)
      entry = (cdir_entry*)((char*)prev + prev->GetSize());
    else
      entry = (cdir_entry*)(mBuf + letoh32(mEnd->cdir_offset));

    if (((char*)entry + entry->GetSize()) > mCdir_limit || !entry->Valid())
      return nullptr;
    return entry;
  }

  string GetEntryName(const cdir_entry* entry) {
    uint16_t len = letoh16(entry->filename_size);

    string name;
    name.append(entry->data, len);
    return name;
  }

  const local_file_header* GetLocalEntry(const cdir_entry* entry) {
    const local_file_header* data =
        (local_file_header*)(mBuf + letoh32(entry->offset));
    if (((char*)data + data->GetSize()) > (char*)mEnd) return nullptr;
    return data;
  }
};

struct AnimationFrame {
  char path[256];
  png_color_16 bgcolor;
  char* buf;
  const local_file_header* file;
  uint32_t width;
  uint32_t height;
  uint16_t bytepp;
  bool has_bgcolor;

  AnimationFrame() : buf(nullptr) {}
  AnimationFrame(const AnimationFrame& frame) : buf(nullptr) {
    strncpy(path, frame.path, sizeof(path));
    file = frame.file;
  }
  ~AnimationFrame() {
    if (buf) free(buf);
  }

  bool operator<(const AnimationFrame& other) const {
    return strcmp(path, other.path) < 0;
  }

  void ReadPngFrame(int outputFormat);
};

struct AnimationPart {
  int32_t count;
  int32_t pause;
  // If you alter the size of the path, change ReadFromString() as well.
  char path[256];
  vector<AnimationFrame> frames;

  bool ReadFromString(const char* aLine) {
    MOZ_ASSERT(aLine);
    // this 255 value must be in sync with AnimationPart::path.
    return sscanf(aLine, "p %d %d %255s", &count, &pause, path) == 3;
  }
};

struct RawReadState {
  const char* start;
  uint32_t offset;
  uint32_t length;
};

bool WriteToSysFile(const char* aFilename, const char* aBuf) {
  size_t aBufSize = strlen(aBuf);
  int fd = open(aFilename, O_WRONLY);
  if (fd < 0) {
    return false;
  }

  ssize_t bytesWritten;
  size_t offset = 0;
  do {
    bytesWritten = write(fd, aBuf + offset, aBufSize - offset);
    if (bytesWritten == -1) {
      return false;
    }
    offset += bytesWritten;
  } while (bytesWritten > 0 && offset < aBufSize);

  return true;
}

void setBacklight(uint32_t aBrightness, bool aIsExternal) {
  if (aBrightness > 100) {
    return;
  }

  // Convert the value in [0, 100] to an int between 0 and 255.
  uint32_t val = aBrightness * 255 / 100;

#define BUFFER_SIZE 100
  char buf[BUFFER_SIZE];
  SprintfLiteral(buf, "%d", val);

  char path[VALUE_MAX_LENGTH];

  if (aIsExternal) {
    property_get("screen.secondary.brightness", path,
                 SECONDARY_SCREEN_BRIGHTNESS);
  } else {
    property_get("screen.primary.brightness", path, PRIMARY_SCREEN_BRIGHTNESS);
  }
  WriteToSysFile(path, buf);
}

static void RawReader(png_structp png_ptr, png_bytep data, png_size_t length) {
  RawReadState* state = (RawReadState*)png_get_io_ptr(png_ptr);
  if (length > (state->length - state->offset))
    png_error(png_ptr, "PNG read overrun");

  memcpy(data, state->start + state->offset, length);
  state->offset += length;
}

static void TransformTo565(png_structp png_ptr, png_row_infop row_info,
                           png_bytep data) {
  uint16_t* outbuf = (uint16_t*)data;
  uint8_t* inbuf = (uint8_t*)data;
  for (uint32_t i = 0; i < row_info->rowbytes; i += 3) {
    *outbuf++ = ((inbuf[i] & 0xF8) << 8) | ((inbuf[i + 1] & 0xFC) << 3) |
                ((inbuf[i + 2]) >> 3);
  }
}

static uint16_t GetFormatBPP(int aFormat) {
  uint16_t bpp = 0;

  switch (aFormat) {
    case HAL_PIXEL_FORMAT_BGRA_8888:
      [[fallthrough]];
    case HAL_PIXEL_FORMAT_RGBA_8888:
      [[fallthrough]];
    case HAL_PIXEL_FORMAT_RGBX_8888:
      bpp = 4;
      break;
    case HAL_PIXEL_FORMAT_RGB_888:
      bpp = 3;
      break;
    case HAL_PIXEL_FORMAT_RGB_565:
      bpp = 2;
      break;
    default:
      LOGW("Unknown pixel format %d. Assuming RGB 565.", aFormat);
      bpp = 2;
  }

  return bpp;
}

void AnimationFrame::ReadPngFrame(int outputFormat) {
#ifdef PNG_HANDLE_AS_UNKNOWN_SUPPORTED
  static const png_byte unused_chunks[] = {99,  72, 82, 77,  '\0',  /* cHRM */
                                           104, 73, 83, 84,  '\0',  /* hIST */
                                           105, 67, 67, 80,  '\0',  /* iCCP */
                                           105, 84, 88, 116, '\0',  /* iTXt */
                                           111, 70, 70, 115, '\0',  /* oFFs */
                                           112, 67, 65, 76,  '\0',  /* pCAL */
                                           115, 67, 65, 76,  '\0',  /* sCAL */
                                           112, 72, 89, 115, '\0',  /* pHYs */
                                           115, 66, 73, 84,  '\0',  /* sBIT */
                                           115, 80, 76, 84,  '\0',  /* sPLT */
                                           116, 69, 88, 116, '\0',  /* tEXt */
                                           116, 73, 77, 69,  '\0',  /* tIME */
                                           122, 84, 88, 116, '\0'}; /* zTXt */
  static const png_byte tRNS_chunk[] = {116, 82, 78, 83, '\0'};     /* tRNS */
#endif

  png_structp pngread =
      png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);

  if (!pngread) return;

  png_infop pnginfo = png_create_info_struct(pngread);

  if (!pnginfo) {
    png_destroy_read_struct(&pngread, &pnginfo, nullptr);
    return;
  }

  if (setjmp(png_jmpbuf(pngread))) {
    // libpng reported an error and longjumped here.  Clean up and return.
    png_destroy_read_struct(&pngread, &pnginfo, nullptr);
    return;
  }

  RawReadState state;
  state.start = file->GetData();
  state.length = file->GetDataSize();
  state.offset = 0;

  png_set_read_fn(pngread, &state, RawReader);

#ifdef PNG_HANDLE_AS_UNKNOWN_SUPPORTED
  /* Ignore unused chunks */
  png_set_keep_unknown_chunks(pngread, 1, unused_chunks,
                              (int)sizeof(unused_chunks) / 5);

  /* Ignore the tRNS chunk if we only want opaque output */
  if (outputFormat == HAL_PIXEL_FORMAT_RGB_888 ||
      outputFormat == HAL_PIXEL_FORMAT_RGB_565) {
    png_set_keep_unknown_chunks(pngread, 1, tRNS_chunk, 1);
  }
#endif

  png_read_info(pngread, pnginfo);

  png_color_16p colorp;
  has_bgcolor = (PNG_INFO_bKGD == png_get_bKGD(pngread, pnginfo, &colorp));
  bgcolor = has_bgcolor ? *colorp : png_color_16();
  width = png_get_image_width(pngread, pnginfo);
  height = png_get_image_height(pngread, pnginfo);

  LOG("Decoded %s: %d x %d frame with bgcolor? %s (%#x, %#x, %#x; gray:%#x)",
      path, width, height, has_bgcolor ? "yes" : "no", bgcolor.red,
      bgcolor.green, bgcolor.blue, bgcolor.gray);

  bytepp = GetFormatBPP(outputFormat);

  switch (outputFormat) {
    case HAL_PIXEL_FORMAT_BGRA_8888:
      png_set_bgr(pngread);
      [[fallthrough]];
    case HAL_PIXEL_FORMAT_RGBA_8888:
      [[fallthrough]];
    case HAL_PIXEL_FORMAT_RGBX_8888:
      png_set_filler(pngread, 0xFF, PNG_FILLER_AFTER);
      break;
    case HAL_PIXEL_FORMAT_RGB_888:
      png_set_strip_alpha(pngread);
      break;
    case HAL_PIXEL_FORMAT_RGB_565:
      png_set_strip_alpha(pngread);
      png_set_read_user_transform_fn(pngread, TransformTo565);
      break;
    default:
      LOGW("Unknown pixel format %d. Assuming RGB 565.", outputFormat);
      png_set_strip_alpha(pngread);
      png_set_read_user_transform_fn(pngread, TransformTo565);
  }

  // An extra row is added to give libpng enough space when
  // decoding 3/4 bytepp inputs for 2 bytepp output surfaces
  buf = (char*)malloc(width * (height + 1) * bytepp);

  vector<char*> rows(height + 1);
  uint32_t stride = width * bytepp;
  for (uint32_t i = 0; i < height; i++) {
    rows[i] = buf + (stride * i);
  }
  rows[height] = nullptr;
  png_set_strip_16(pngread);
  png_set_palette_to_rgb(pngread);
  png_set_gray_to_rgb(pngread);
  png_read_image(pngread, (png_bytepp)&rows.front());
  png_destroy_read_struct(&pngread, &pnginfo, nullptr);
}

struct Animation {
  ZipReader reader;

  // Set by loaded file
  int32_t width;
  int32_t height;
  int32_t fps;
  vector<AnimationPart> parts;

  // Set by user
  int32_t format;
  DisplayType dpy;

  Animation();

  // return true on success
  bool LoadAnimations(const char* fileName);
  bool CanPlaySimultaneously(Animation& ext);
};

Animation::Animation()
    : width(320),
      height(480),
      fps(12),
      format(HAL_PIXEL_FORMAT_RGBX_8888),
      dpy(DisplayType::DISPLAY_PRIMARY) {}

bool Animation::LoadAnimations(const char* aFileName) {
  if (!reader.OpenArchive(aFileName)) {
    LOGW("Could not open boot animation");
    return false;
  }

  const cdir_entry* entry = nullptr;
  const local_file_header* file = nullptr;
  while ((entry = reader.GetNextEntry(entry))) {
    string name = reader.GetEntryName(entry);
    if (!name.compare("desc.txt")) {
      file = reader.GetLocalEntry(entry);
      break;
    }
  }

  if (!file) {
    LOGW("Could not find desc.txt in boot animation");
    return false;
  }

  string descCopy;
  descCopy.append(file->GetData(), entry->GetDataSize());
  const char* line = descCopy.c_str();
  const char* end;
  bool headerRead = true;

  /*
   * bootanimation.zip
   *
   * This is the boot animation file format that Android uses.
   * It's a zip file with a directories containing png frames
   * and a desc.txt that describes how they should be played.
   *
   * desc.txt contains two types of lines
   * 1. [width] [height] [fps]
   *    There is one of these lines per bootanimation.
   *    If the width and height are smaller than the screen,
   *    the frames are centered on a black background.
   *    XXX: Currently we stretch instead of centering the frame.
   * 2. p [count] [pause] [path]
   *    This describes one animation part.
   *    Each animation part is played in sequence.
   *    An animation part contains all the files/frames in the
   *    directory specified in [path]
   *    [count] indicates the number of times this part repeats.
   *    [pause] indicates the number of frames that this part
   *    should pause for after playing the full sequence but
   *    before repeating.
   */

  do {
    end = strstr(line, "\n");

    AnimationPart part;
    if (headerRead && sscanf(line, "%d %d %d", &width, &height, &fps) == 3) {
      headerRead = false;
    } else if (sscanf(line, "p %d %d %s", &part.count, &part.pause,
                      part.path)) {
      parts.push_back(part);
    }
  } while (end && *(line = end + 1));

  for (uint32_t i = 0; i < parts.size(); i++) {
    AnimationPart& part = parts[i];
    entry = nullptr;
    char search[256];
    SprintfLiteral(search, "%s/", part.path);
    while ((entry = reader.GetNextEntry(entry))) {
      string name = reader.GetEntryName(entry);
      if (name.find(search) || !entry->GetDataSize() || name.length() >= 256)
        continue;

      part.frames.push_back(AnimationFrame());
      AnimationFrame& frame = part.frames.back();
      strcpy(frame.path, name.c_str());
      frame.file = reader.GetLocalEntry(entry);
    }

    sort(part.frames.begin(), part.frames.end());
  }

  return true;
}

bool Animation::CanPlaySimultaneously(Animation& aExt) {
  if (fps != aExt.fps) {
    return false;
  }

  if (parts.size() != aExt.parts.size()) {
    return false;
  }

  for (uint32_t i = 0; i < parts.size(); i++) {
    if (parts[i].pause != aExt.parts[i].pause ||
        parts[i].count != aExt.parts[i].count ||
        parts[i].frames.size() != aExt.parts[i].frames.size()) {
      return false;
    }
  }

  return true;
}

/**
 * Return a wchar_t that when used to |wmemset()| an image buffer will
 * fill it with the color defined by |color16|.  The packed wchar_t
 * may comprise one or two pixels depending on |outputFormat|.
 */
static wchar_t AsBackgroundFill(const png_color_16& color16, int outputFormat) {
  static_assert(sizeof(wchar_t) == sizeof(uint32_t),
                "TODO: support 2-byte wchar_t");
  union {
    uint32_t r8g8b8;
    struct {
      uint8_t b8;
      uint8_t g8;
      uint8_t r8;
      uint8_t x8;
    };
  } color;
  color.b8 = color16.blue;
  color.g8 = color16.green;
  color.r8 = color16.red;
  color.x8 = 0xFF;

  switch (outputFormat) {
    case HAL_PIXEL_FORMAT_RGBA_8888:
      [[fallthrough]];
    case HAL_PIXEL_FORMAT_RGBX_8888:
      return color.r8g8b8;

    case HAL_PIXEL_FORMAT_BGRA_8888:
      swap(color.r8, color.b8);
      return color.r8g8b8;

    case HAL_PIXEL_FORMAT_RGB_565: {
      // NB: we could do a higher-quality downsample here, but we
      // want the results to be a pixel-perfect match with the fast
      // downsample in TransformTo565().
      uint16_t color565 = ((color.r8 & 0xF8) << 8) | ((color.g8 & 0xFC) << 3) |
                          ((color.b8) >> 3);
      return (color565 << 16) | color565;
    }
    default:
      LOGW("Unhandled pixel format %d; falling back on black", outputFormat);
      return 0;
  }
}

static void ShowSolidColorFrame(GonkDisplay* aDisplay, int32_t aFormat,
                                DisplayType aDpy) {
  LOGW("Show solid color frame for bootAnim");

  ANativeWindowBuffer* buffer = aDisplay->DequeueBuffer(aDpy);
  void* mappedAddress = nullptr;

  if (!buffer) {
    LOGW("Failed to get an ANativeWindowBuffer");
    return;
  }
  if (android::GraphicBuffer::from(buffer)->lock(
          GRALLOC_USAGE_SW_READ_NEVER | GRALLOC_USAGE_SW_WRITE_OFTEN |
              GRALLOC_USAGE_HW_FB,
          &mappedAddress) == android::OK) {
    // Just show a black solid color frame.
    memset(mappedAddress, 0,
           buffer->height * buffer->stride * GetFormatBPP(aFormat));
    android::GraphicBuffer::from(buffer)->unlock();
  }

  aDisplay->QueueBuffer(buffer, aDpy);
}

static bool DrawFrame(AnimationFrame& aFrame, ANativeWindowBuffer* aBuf,
                      int32_t format, void* aVaddr) {
  if (!aBuf || !aFrame.buf) {
    return false;
  }

  uint32_t bufWidth = aBuf->width;
  uint32_t bufHeight = aBuf->height;
  uint32_t bufStride = aBuf->stride;

  if (aFrame.has_bgcolor) {
    wchar_t bgfill = AsBackgroundFill(aFrame.bgcolor, format);
    wmemset((wchar_t*)aVaddr, bgfill,
            (bufHeight * bufStride * aFrame.bytepp) / sizeof(wchar_t));
  }

  if (bufHeight == aFrame.height && bufStride == aFrame.width) {
    memcpy(aVaddr, aFrame.buf, aFrame.width * aFrame.height * aFrame.bytepp);
  } else if (bufHeight >= aFrame.height && bufWidth >= aFrame.width) {
    int startx = (bufWidth - aFrame.width) / 2;
    int starty = (bufHeight - aFrame.height) / 2;

    int src_stride = aFrame.width * aFrame.bytepp;
    int dst_stride = bufStride * aFrame.bytepp;

    char* src = aFrame.buf;
    char* dst = (char*)aVaddr + starty * dst_stride + startx * aFrame.bytepp;

    for (uint32_t i = 0; i < aFrame.height; i++) {
      memcpy(dst, src, src_stride);
      src += src_stride;
      dst += dst_stride;
    }
  }

  return true;
}

static void* AnimationThread(void*) {
  GonkDisplay* display = GetGonkDisplay();

  const GonkDisplay::DisplayNativeData& dispData =
      display->GetDispNativeData(DisplayType::DISPLAY_PRIMARY);

  const GonkDisplay::DisplayNativeData& extDispData =
      display->GetDispNativeData(DisplayType::DISPLAY_EXTERNAL);

  vector<Animation> animVec;

  // Load boot animation for primary screen
  animVec.push_back(Animation());
  Animation& primAnimation = animVec.back();
  primAnimation.dpy = DisplayType::DISPLAY_PRIMARY;
  primAnimation.format = dispData.mSurfaceformat;
  if (!primAnimation.LoadAnimations("/system/media/bootanimation.zip")) {
    LOGW("Failed to load boot animation file for primary screen");
    ShowSolidColorFrame(display, dispData.mSurfaceformat,
                        DisplayType::DISPLAY_PRIMARY);
    return nullptr;
  }

  if (display->IsExtFBDeviceEnabled()) {
    animVec.push_back(Animation());
    Animation& extAnimation = animVec.back();
    extAnimation.dpy = DisplayType::DISPLAY_EXTERNAL;
    extAnimation.format = extDispData.mSurfaceformat;
    if (!extAnimation.LoadAnimations(
            "/system/media/bootanimation_external.zip") ||
        !animVec[0].CanPlaySimultaneously(extAnimation)) {
      LOGW("Failed to load boot animation file for external screen");
      ShowSolidColorFrame(display, extDispData.mSurfaceformat,
                          DisplayType::DISPLAY_EXTERNAL);
      animVec.pop_back();
    } else {
      // Turn on external screen backlight before playing animation and
      // draw a solid frame to clear noise on panel.
      ShowSolidColorFrame(display, extDispData.mSurfaceformat,
                          DisplayType::DISPLAY_EXTERNAL);
      usleep(20000);
      setBacklight(100, true);
    }
  }
  // Turn on primary screen backlight before playing animation,
  setBacklight(100, false);
  bool animPlayed = false;

  /* We assume boot animation for external screen would have the same
     parts and frames with of primary screen, so we use primAnimation's
     attribute to control the painting loop.
  */

  // Reref primary in case implicit data manipulate of vector
  Animation& mainAnim = animVec.front();
  long frameDelayUs = 1000000 / mainAnim.fps;
  uint32_t numAnim = animVec.size();

  for (uint32_t i = 0; i < mainAnim.parts.size(); i++) {
    AnimationPart& part = mainAnim.parts[i];

    int32_t j = 0;
    while (sRunAnimation && (!part.count || j++ < part.count)) {
      for (uint32_t k = 0; k < part.frames.size(); k++) {
        struct timeval tv1, tv2;
        gettimeofday(&tv1, nullptr);

        for (uint32_t s = 0; s < numAnim; s++) {
          Animation& anim = animVec[s];
          AnimationFrame& frame = anim.parts[i].frames[k];
          if (!frame.buf) {
            frame.ReadPngFrame(anim.format);
          }

          ANativeWindowBuffer* buf = display->DequeueBuffer(anim.dpy);

          if (!buf) {
            LOGW("Failed to get an ANativeWindowBuffer");
          }

          void* vaddr = nullptr;
          if (android::GraphicBuffer::from(buf)->lock(
                  GRALLOC_USAGE_SW_READ_NEVER | GRALLOC_USAGE_SW_WRITE_OFTEN |
                      GRALLOC_USAGE_HW_FB,
                  &vaddr) != android::OK) {
            LOGW("Failed to lock GraphicBuffer");
            display->QueueBuffer(buf, anim.dpy);
            buf = nullptr;
          }

          if (!buf || !DrawFrame(frame, buf, anim.format, vaddr)) {
            LOGW("Failed to Draw Frame on Display[%d] of Part[%d] of Frame[%d]",
                 s, i, k);
          }

          animPlayed = true;

          if (buf) {
            android::GraphicBuffer::from(buf)->unlock();
            display->QueueBuffer(buf, anim.dpy);
          }

          if (part.count && j >= part.count) {
            free(frame.buf);
            frame.buf = nullptr;
          }
        }

        gettimeofday(&tv2, nullptr);

        timersub(&tv2, &tv1, &tv2);

        if (tv2.tv_usec < frameDelayUs) {
          usleep(frameDelayUs - tv2.tv_usec);
        } else {
          LOGW("Frame delay is %ld us but decoding took %ld us", frameDelayUs,
               tv2.tv_usec);
        }
      }
      usleep(frameDelayUs * part.pause);
    }
  }

  if (!animPlayed) {
    ShowSolidColorFrame(display, dispData.mSurfaceformat,
                        DisplayType::DISPLAY_PRIMARY);
    if (display->IsExtFBDeviceEnabled()) {
      ShowSolidColorFrame(display, extDispData.mSurfaceformat,
                          DisplayType::DISPLAY_EXTERNAL);
    }
  }
  if (animPlayed && display->IsExtFBDeviceEnabled() && numAnim > 1) {
    usleep(500000);
    setBacklight(0, true);
  }
  return nullptr;
}

namespace mozilla {

__attribute__((visibility("default"))) void StartBootAnimation() {
  GetGonkDisplay();  // Ensure GonkDisplay exist
  sRunAnimation = true;
  HookSetVsyncAlwaysEnabled(true);
  pthread_create(&sAnimationThread, nullptr, AnimationThread, nullptr);
}

__attribute__((visibility("default"))) void StopBootAnimation() {
  if (sRunAnimation) {
    sRunAnimation = false;
    HookSetVsyncAlwaysEnabled(false);
    pthread_join(sAnimationThread, nullptr);
    GetGonkDisplay()->NotifyBootAnimationStopped();
  }
}

}  // namespace mozilla
