/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "B2GH265.h"
#include <cmath>
#include <limits>
#include "AnnexB.h"
#include "BitReader.h"
#include "BitWriter.h"
#include "BufferReader.h"
#include "ByteWriter.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/PodOperations.h"
#include "mozilla/ResultExtensions.h"

#define CHECK(cond) \
  do {              \
    if (!(cond)) {  \
      return false; \
    }               \
  } while (false)

#define CHECK_RANGE(var, min, max) CHECK((var) >= (min) && (var) <= (max))

namespace mozilla {

B2GH265::VPSData::VPSData() { PodZero(this); }

B2GH265::SPSData::SPSData() { PodZero(this); }

// Described in ISO 23001-8:2016
// Table 2
enum class PrimaryID : uint8_t {
  INVALID = 0,
  BT709 = 1,
  UNSPECIFIED = 2,
  BT470M = 4,
  BT470BG = 5,
  SMPTE170M = 6,
  SMPTE240M = 7,
  FILM = 8,
  BT2020 = 9,
  SMPTEST428_1 = 10,
  SMPTEST431_2 = 11,
  SMPTEST432_1 = 12,
  EBU_3213_E = 22
};

// Table 3
enum class TransferID : uint8_t {
  INVALID = 0,
  BT709 = 1,
  UNSPECIFIED = 2,
  GAMMA22 = 4,
  GAMMA28 = 5,
  SMPTE170M = 6,
  SMPTE240M = 7,
  LINEAR = 8,
  LOG = 9,
  LOG_SQRT = 10,
  IEC61966_2_4 = 11,
  BT1361_ECG = 12,
  IEC61966_2_1 = 13,
  BT2020_10 = 14,
  BT2020_12 = 15,
  SMPTEST2084 = 16,
  SMPTEST428_1 = 17,

  // Not yet standardized
  ARIB_STD_B67 = 18,  // AKA hybrid-log gamma, HLG.
};

// Table 4
enum class MatrixID : uint8_t {
  RGB = 0,
  BT709 = 1,
  UNSPECIFIED = 2,
  FCC = 4,
  BT470BG = 5,
  SMPTE170M = 6,
  SMPTE240M = 7,
  YCOCG = 8,
  BT2020_NCL = 9,
  BT2020_CL = 10,
  YDZDX = 11,
  INVALID = 255,
};

static PrimaryID GetPrimaryID(int aPrimary) {
  if (aPrimary < 1 || aPrimary > 22 || aPrimary == 3) {
    return PrimaryID::INVALID;
  }
  if (aPrimary > 12 && aPrimary < 22) {
    return PrimaryID::INVALID;
  }
  return static_cast<PrimaryID>(aPrimary);
}

static TransferID GetTransferID(int aTransfer) {
  if (aTransfer < 1 || aTransfer > 18 || aTransfer == 3) {
    return TransferID::INVALID;
  }
  return static_cast<TransferID>(aTransfer);
}

static MatrixID GetMatrixID(int aMatrix) {
  if (aMatrix < 0 || aMatrix > 11 || aMatrix == 3) {
    return MatrixID::INVALID;
  }
  return static_cast<MatrixID>(aMatrix);
}

gfx::YUVColorSpace B2GH265::SPSData::ColorSpace() const {
  // Bitfield, note that guesses with higher values take precedence over
  // guesses with lower values.
  enum Guess {
    GUESS_BT601 = 1 << 0,
    GUESS_BT709 = 1 << 1,
    GUESS_BT2020 = 1 << 2,
  };

  uint32_t guess = 0;

  switch (GetPrimaryID(vui.colour_primaries)) {
    case PrimaryID::BT709:
      guess |= GUESS_BT709;
      break;
    case PrimaryID::BT470M:
    case PrimaryID::BT470BG:
    case PrimaryID::SMPTE170M:
    case PrimaryID::SMPTE240M:
      guess |= GUESS_BT601;
      break;
    case PrimaryID::BT2020:
      guess |= GUESS_BT2020;
      break;
    case PrimaryID::FILM:
    case PrimaryID::SMPTEST428_1:
    case PrimaryID::SMPTEST431_2:
    case PrimaryID::SMPTEST432_1:
    case PrimaryID::EBU_3213_E:
    case PrimaryID::INVALID:
    case PrimaryID::UNSPECIFIED:
      break;
  }

  switch (GetTransferID(vui.transfer_characteristics)) {
    case TransferID::BT709:
      guess |= GUESS_BT709;
      break;
    case TransferID::GAMMA22:
    case TransferID::GAMMA28:
    case TransferID::SMPTE170M:
    case TransferID::SMPTE240M:
      guess |= GUESS_BT601;
      break;
    case TransferID::BT2020_10:
    case TransferID::BT2020_12:
      guess |= GUESS_BT2020;
      break;
    case TransferID::LINEAR:
    case TransferID::LOG:
    case TransferID::LOG_SQRT:
    case TransferID::IEC61966_2_4:
    case TransferID::BT1361_ECG:
    case TransferID::IEC61966_2_1:
    case TransferID::SMPTEST2084:
    case TransferID::SMPTEST428_1:
    case TransferID::ARIB_STD_B67:
    case TransferID::INVALID:
    case TransferID::UNSPECIFIED:
      break;
  }

  switch (GetMatrixID(vui.matrix_coefficients)) {
    case MatrixID::BT709:
      guess |= GUESS_BT709;
      break;
    case MatrixID::BT470BG:
    case MatrixID::SMPTE170M:
    case MatrixID::SMPTE240M:
      guess |= GUESS_BT601;
      break;
    case MatrixID::BT2020_NCL:
    case MatrixID::BT2020_CL:
      guess |= GUESS_BT2020;
      break;
    case MatrixID::RGB:
    case MatrixID::FCC:
    case MatrixID::YCOCG:
    case MatrixID::YDZDX:
    case MatrixID::INVALID:
    case MatrixID::UNSPECIFIED:
      break;
  }

  // Removes lowest bit until only a single bit remains.
  while (guess & (guess - 1)) {
    guess &= guess - 1;
  }
  if (!guess) {
    // A better default to BT601 which should die a slow death.
    guess = GUESS_BT709;
  }

  switch (guess) {
    case GUESS_BT601:
      return gfx::YUVColorSpace::BT601;
    case GUESS_BT709:
      return gfx::YUVColorSpace::BT709;
    case GUESS_BT2020:
      return gfx::YUVColorSpace::BT2020;
    default:
      MOZ_CRASH("not possible to get here but makes compiler happy");
  }
}

gfx::ColorDepth B2GH265::SPSData::ColorDepth() const {
  if (bit_depth_luma != 8 && bit_depth_luma != 10 && bit_depth_luma != 12) {
    // We don't know what that is, just assume 8 bits to prevent decoding
    // regressions if we ever encounter those.
    return gfx::ColorDepth::COLOR_8;
  }
  return gfx::ColorDepthForBitDepth(bit_depth_luma);
}

/* static */
already_AddRefed<MediaByteBuffer> B2GH265::DecodeNALUnit(const uint8_t* aNAL,
                                                         size_t aLength) {
  MOZ_ASSERT(aNAL);

  if (aLength < 2) {
    return nullptr;
  }

  RefPtr<MediaByteBuffer> rbsp = new MediaByteBuffer;
  BufferReader reader(aNAL, aLength);
  auto res = reader.ReadU16();
  if (res.isErr()) {
    return nullptr;
  }

  uint32_t lastbytes = 0xffff;
  while (reader.Remaining()) {
    auto res = reader.ReadU8();
    if (res.isErr()) {
      return nullptr;
    }
    uint8_t byte = res.unwrap();
    if ((lastbytes & 0xffff) == 0 && byte == 0x03) {
      // reset last two bytes, to detect the 0x000003 sequence again.
      lastbytes = 0xffff;
    } else {
      rbsp->AppendElement(byte);
    }
    lastbytes = (lastbytes << 8) | byte;
  }
  return rbsp.forget();
}

/* static */
bool B2GH265::DecodeNALUnitFromExtraData(
    const MediaByteBuffer* aExtraData, uint8_t aNALType,
    const std::function<bool(const uint8_t*, size_t)>& aNALDecoder) {
  BufferReader reader(aExtraData->Elements(), aExtraData->Length());
  CHECK(reader.Seek(22));

  uint8_t numOfArrays = reader.ReadU8().unwrapOr(0);
  for (int i = 0; i < numOfArrays; i++) {
    uint8_t type = reader.ReadU8().unwrapOr(0) & 0x3f;
    uint16_t numOfNALs = reader.ReadU16().unwrapOr(0);
    for (int j = 0; j < numOfNALs; j++) {
      uint16_t len = reader.ReadU16().unwrapOr(0);
      size_t offset = reader.Offset();
      CHECK(offset + len <= aExtraData->Length());
      if (type == aNALType) {
        return aNALDecoder(aExtraData->Elements() + offset, len);
      }
      CHECK(reader.Seek(offset + len));
    }
  }
  return false;
}

/* static */
bool B2GH265::DecodeVPSFromExtraData(const MediaByteBuffer* aExtraData,
                                     VPSData& aDest) {
  return DecodeNALUnitFromExtraData(
      aExtraData, H265_NAL_VPS, [&aDest](const uint8_t* aNAL, size_t aLength) {
        RefPtr<MediaByteBuffer> rbsp = DecodeNALUnit(aNAL, aLength);
        return DecodeVPS(rbsp, aDest);
      });
}

/* static */
bool B2GH265::DecodeSPSFromExtraData(const MediaByteBuffer* aExtraData,
                                     SPSData& aDest) {
  return DecodeNALUnitFromExtraData(
      aExtraData, H265_NAL_SPS, [&aDest](const uint8_t* aNAL, size_t aLength) {
        RefPtr<MediaByteBuffer> rbsp = DecodeNALUnit(aNAL, aLength);
        return DecodeSPS(rbsp, aDest);
      });
}

/* static */
bool B2GH265::DecodeVPS(const MediaByteBuffer* aVPS, VPSData& aDest) {
  CHECK(aVPS);

  BitReader br(aVPS, BitReader::GetBitLength(aVPS));
  return video_parameter_set_rbsp(br, aDest);
}

/* static */
bool B2GH265::DecodeSPS(const MediaByteBuffer* aSPS, SPSData& aDest) {
  CHECK(aSPS);

  BitReader br(aSPS, BitReader::GetBitLength(aSPS));
  return seq_parameter_set_rbsp(br, aDest);
}

static uint32_t ReadNalLen(BufferReader& aBr, int aNalLenSize) {
  uint32_t nalLen = 0;
  switch (aNalLenSize) {
    case 1:
      nalLen = aBr.ReadU8().unwrapOr(0);
      break;
    case 2:
      nalLen = aBr.ReadU16().unwrapOr(0);
      break;
    case 3:
      nalLen = aBr.ReadU24().unwrapOr(0);
      break;
    case 4:
      nalLen = aBr.ReadU32().unwrapOr(0);
      break;
  }
  return nalLen;
}

/* static */
B2GH265::FrameType B2GH265::GetFrameType(const MediaRawData* aSample) {
  if (!H265AnnexB::IsHVCC(aSample)) {
    // We must have a valid HVCC frame with extradata.
    return FrameType::INVALID;
  }
  MOZ_ASSERT(aSample->Data());

  int nalLenSize = ((*aSample->mExtraData)[21] & 3) + 1;

  BufferReader reader(aSample->Data(), aSample->Size());

  while (reader.Remaining() >= nalLenSize) {
    uint32_t nalLen = ReadNalLen(reader, nalLenSize);
    if (!nalLen) {
      continue;
    }
    const uint8_t* p = reader.Read(nalLen);
    if (!p) {
      return FrameType::INVALID;
    }
    uint8_t nalType = (*p >> 1) & 0x3f;
    if (nalType >= 16 && nalType <= 23) {
      // IRAP NAL
      return FrameType::I_FRAME;
    }
  }
  return FrameType::OTHER;
}

/* static */
bool B2GH265::video_parameter_set_rbsp(BitReader& aBr, VPSData& aDest) {
  aDest.vps_video_parameter_set_id = aBr.ReadBits(4);
  aDest.vps_base_layer_internal_flag = aBr.ReadBit();
  aDest.vps_base_layer_available_flag = aBr.ReadBit();
  aDest.vps_max_layers = aBr.ReadBits(6) + 1;
  aDest.vps_max_sub_layers = aBr.ReadBits(3) + 1;
  CHECK_RANGE(aDest.vps_max_sub_layers, 1, HEVC_MAX_SUB_LAYERS);

  aDest.vps_temporal_id_nesting_flag = aBr.ReadBit();
  aBr.ReadBits(16);  // vps_reserved_0xffff_16bits

  CHECK(profile_tier_level(aBr, aDest.ptl, true, aDest.vps_max_sub_layers));

  aDest.vps_sub_layer_ordering_info_present_flag = aBr.ReadBit();

  uint32_t i = aDest.vps_sub_layer_ordering_info_present_flag
                   ? 0
                   : aDest.vps_max_sub_layers - 1;
  for (; i < aDest.vps_max_sub_layers; i++) {
    aDest.vps_max_dec_pic_buffering[i] = aBr.ReadUE() + 1;
    aDest.vps_max_num_reorder_pics[i] = aBr.ReadUE();
    aDest.vps_max_latency_increase[i] = aBr.ReadUE() - 1;
    CHECK_RANGE(aDest.vps_max_dec_pic_buffering[i], 1, HEVC_MAX_DPB_SIZE);
    CHECK_RANGE(aDest.vps_max_num_reorder_pics[i], 0,
                aDest.vps_max_dec_pic_buffering[i] - 1);
  }

  aDest.vps_max_layer_id = aBr.ReadBits(6);
  aDest.vps_num_layer_sets = aBr.ReadUE() + 1;
  CHECK_RANGE(aDest.vps_num_layer_sets, 1, 1024);

  for (i = 1; i < aDest.vps_num_layer_sets; i++) {
    for (uint32_t j = 0; j <= aDest.vps_max_layer_id; j++) {
      aBr.ReadBit();  // layer_id_included_flag[i][j]
    }
  }

  aDest.vps_timing_info_present_flag = aBr.ReadBit();
  if (aDest.vps_timing_info_present_flag) {
    aDest.vps_num_units_in_tick = aBr.ReadBits(32);
    aDest.vps_time_scale = aBr.ReadBits(32);
    aDest.vps_poc_proportional_to_timing_flag = aBr.ReadBit();
    if (aDest.vps_poc_proportional_to_timing_flag) {
      aDest.vps_num_ticks_poc_diff_one = aBr.ReadUE() + 1;
      CHECK_RANGE(aDest.vps_num_ticks_poc_diff_one, 1, UINT32_MAX);
    }
    aDest.vps_num_hrd_parameters = aBr.ReadUE();
    CHECK_RANGE(aDest.vps_num_hrd_parameters, 0, aDest.vps_num_layer_sets);
    for (i = 0; i < aDest.vps_num_hrd_parameters; i++) {
      uint32_t hrd_layer_set_idx = aBr.ReadUE();
      CHECK_RANGE(hrd_layer_set_idx, aDest.vps_base_layer_internal_flag ? 0 : 1,
                  aDest.vps_num_layer_sets - 1);
      bool cprms_present_flag = true;
      if (i > 0) {
        cprms_present_flag = aBr.ReadBit();
      }
      CHECK(hrd_parameters(aBr, cprms_present_flag, aDest.vps_max_sub_layers));
    }
  }

  // Skip vps_extension_flag and the rest.
  return true;
}

/* static */
bool B2GH265::seq_parameter_set_rbsp(BitReader& aBr, SPSData& aDest) {
  aDest.sps_video_parameter_set_id = aBr.ReadBits(4);
  aDest.sps_max_sub_layers = aBr.ReadBits(3) + 1;
  CHECK_RANGE(aDest.sps_max_sub_layers, 1, 7);
  aDest.sps_temporal_id_nesting_flag = aBr.ReadBit();

  CHECK(profile_tier_level(aBr, aDest.ptl, true, aDest.sps_max_sub_layers));

  aDest.sps_seq_parameter_set_id = aBr.ReadUE();
  CHECK_RANGE(aDest.sps_seq_parameter_set_id, 0, HEVC_MAX_SPS_COUNT - 1);

  aDest.chroma_format_idc = aBr.ReadUE();
  CHECK_RANGE(aDest.chroma_format_idc, 0, 3);

  if (aDest.chroma_format_idc == 3) {
    aDest.separate_colour_plane_flag = aBr.ReadBit();
  }

  aDest.pic_width_in_luma_samples = aBr.ReadUE();
  aDest.pic_height_in_luma_samples = aBr.ReadUE();
  CHECK(aDest.pic_width_in_luma_samples > 0);
  CHECK(aDest.pic_height_in_luma_samples > 0);

  aDest.conformance_window_flag = aBr.ReadBit();
  if (aDest.conformance_window_flag) {
    aDest.conf_win_left_offset = aBr.ReadUE();
    aDest.conf_win_right_offset = aBr.ReadUE();
    aDest.conf_win_top_offset = aBr.ReadUE();
    aDest.conf_win_bottom_offset = aBr.ReadUE();
  }

  aDest.bit_depth_luma = aBr.ReadUE() + 8;
  aDest.bit_depth_chroma = aBr.ReadUE() + 8;
  CHECK_RANGE(aDest.bit_depth_luma, 8, 16);
  CHECK_RANGE(aDest.bit_depth_chroma, 8, 16);

  aDest.log2_max_pic_order_cnt_lsb = aBr.ReadUE() + 4;
  CHECK_RANGE(aDest.log2_max_pic_order_cnt_lsb, 4, 16);

  bool sps_sub_layer_ordering_info_present_flag = aBr.ReadBit();
  int start = sps_sub_layer_ordering_info_present_flag
                  ? 0
                  : aDest.sps_max_sub_layers - 1;
  for (int i = start; i < aDest.sps_max_sub_layers; i++) {
    aDest.sps_max_dec_pic_buffering[i] = aBr.ReadUE() + 1;
    aDest.sps_max_num_reorder_pics[i] = aBr.ReadUE();
    aDest.sps_max_latency_increase[i] = aBr.ReadUE() - 1;
    CHECK_RANGE(aDest.sps_max_dec_pic_buffering[i], 1, HEVC_MAX_DPB_SIZE);
    CHECK_RANGE(aDest.sps_max_num_reorder_pics[i], 0,
                aDest.sps_max_dec_pic_buffering[i] - 1);
  }

  if (!sps_sub_layer_ordering_info_present_flag) {
    for (int i = 0; i < start; i++) {
      aDest.sps_max_dec_pic_buffering[i] =
          aDest.sps_max_dec_pic_buffering[start];
      aDest.sps_max_num_reorder_pics[i] = aDest.sps_max_num_reorder_pics[start];
      aDest.sps_max_latency_increase[i] = aDest.sps_max_latency_increase[start];
    }
  }

  aDest.log2_min_luma_coding_block_size = aBr.ReadUE() + 3;
  aDest.log2_diff_max_min_luma_coding_block_size = aBr.ReadUE();
  aDest.log2_min_luma_transform_block_size = aBr.ReadUE() + 2;
  aDest.log2_diff_max_min_luma_transform_block_size = aBr.ReadUE();

  aDest.max_transform_hierarchy_depth_inter = aBr.ReadUE();
  aDest.max_transform_hierarchy_depth_intra = aBr.ReadUE();

  aDest.scaling_list_enable_flag = aBr.ReadBit();
  if (aDest.scaling_list_enable_flag) {
    aDest.sps_scaling_list_data_present_flag = aBr.ReadBit();
    if (aDest.sps_scaling_list_data_present_flag) {
      CHECK(scaling_list_data(aBr, aDest.scaling_list));
    }
  }

  aDest.amp_enabled_flag = aBr.ReadBit();
  aDest.sample_adaptive_offset_enabled_flag = aBr.ReadBit();

  aDest.pcm_enabled_flag = aBr.ReadBit();
  if (aDest.pcm_enabled_flag) {
    aDest.pcm_sample_bit_depth_luma = aBr.ReadBits(4) + 1;
    aDest.pcm_sample_bit_depth_chroma = aBr.ReadBits(4) + 1;
    aDest.log2_min_pcm_luma_coding_block_size = aBr.ReadUE() + 3;
    aDest.log2_diff_max_min_pcm_luma_coding_block_size = aBr.ReadUE();
    aDest.pcm_loop_filter_disabled_flag = aBr.ReadBit();
    CHECK_RANGE(aDest.pcm_sample_bit_depth_luma, 1, aDest.bit_depth_luma);
    CHECK_RANGE(aDest.pcm_sample_bit_depth_chroma, 1, aDest.bit_depth_chroma);
  }

  aDest.num_short_term_ref_pic_sets = aBr.ReadUE();
  CHECK_RANGE(aDest.num_short_term_ref_pic_sets, 0, 64);
  for (uint32_t i = 0; i < aDest.num_short_term_ref_pic_sets; i++) {
    CHECK(st_ref_pic_set(aBr, aDest.st_rps[i], aDest, i));
  }

  aDest.long_term_ref_pics_present_flag = aBr.ReadBit();
  if (aDest.long_term_ref_pics_present_flag) {
    aDest.num_long_term_ref_pics_sps = aBr.ReadUE();
    CHECK_RANGE(aDest.num_long_term_ref_pics_sps, 0, 32);
    for (uint32_t i = 0; i < aDest.num_long_term_ref_pics_sps; i++) {
      aDest.lt_ref_pic_poc_lsb_sps[i] =
          aBr.ReadBits(aDest.log2_max_pic_order_cnt_lsb);
      aDest.used_by_curr_pic_lt_sps_flag[i] = aBr.ReadBit();
    }
  }

  aDest.sps_temporal_mvp_enabled_flag = aBr.ReadBit();
  aDest.sps_strong_intra_smoothing_enable_flag = aBr.ReadBit();
  bool vui_present_flag = aBr.ReadBit();
  if (vui_present_flag) {
    CHECK(vui_parameters(aBr, aDest.vui, aDest.sps_max_sub_layers));
  }

  // Skip sps_extension_flag and the rest.
  return true;
}

/* static */
bool B2GH265::profile_tier_level(BitReader& aBr, PTLData& aDest,
                                 bool aProfilePresentFlag,
                                 uint8_t aMaxNumSubLayers) {
  if (aProfilePresentFlag) {
    aDest.general_ptl.profile_space = aBr.ReadBits(2);
    aDest.general_ptl.tier_flag = aBr.ReadBit();
    aDest.general_ptl.profile_idc = aBr.ReadBits(5);
    for (int j = 0; j < 32; j++) {
      aDest.general_ptl.profile_compatibility_flag[j] = aBr.ReadBit();
    }
    aDest.general_ptl.progressive_source_flag = aBr.ReadBit();
    aDest.general_ptl.interlaced_source_flag = aBr.ReadBit();
    aDest.general_ptl.non_packed_constraint_flag = aBr.ReadBit();
    aDest.general_ptl.frame_only_constraint_flag = aBr.ReadBit();

    auto check_profile_idc = [&aDest](int idc) {
      return aDest.general_ptl.profile_idc == idc ||
             aDest.general_ptl.profile_compatibility_flag[idc];
    };

    if (check_profile_idc(4) || check_profile_idc(5) || check_profile_idc(6) ||
        check_profile_idc(7) || check_profile_idc(8) || check_profile_idc(9) ||
        check_profile_idc(10) || check_profile_idc(11)) {
      aDest.general_ptl.max_12bit_constraint_flag = aBr.ReadBit();
      aDest.general_ptl.max_10bit_constraint_flag = aBr.ReadBit();
      aDest.general_ptl.max_8bit_constraint_flag = aBr.ReadBit();
      aDest.general_ptl.max_422chroma_constraint_flag = aBr.ReadBit();
      aDest.general_ptl.max_420chroma_constraint_flag = aBr.ReadBit();
      aDest.general_ptl.max_monochrome_constraint_flag = aBr.ReadBit();
      aDest.general_ptl.intra_constraint_flag = aBr.ReadBit();
      aDest.general_ptl.one_picture_only_constraint_flag = aBr.ReadBit();
      aDest.general_ptl.lower_bit_rate_constraint_flag = aBr.ReadBit();
      if (check_profile_idc(5) || check_profile_idc(9) ||
          check_profile_idc(10) || check_profile_idc(11)) {
        aDest.general_ptl.max_14bit_constraint_flag = aBr.ReadBit();
        aBr.ReadBits(33);  // general_reserved_zero_33bits
      } else {
        aBr.ReadBits(34);  // general_reserved_zero_34bits
      }
    } else if (check_profile_idc(2)) {
      aBr.ReadBits(7);  // general_reserved_zero_7bits
      aDest.general_ptl.one_picture_only_constraint_flag = aBr.ReadBit();
      aBr.ReadBits(35);  // general_reserved_zero_35bits
    } else {
      aBr.ReadBits(43);  // general_reserved_zero_43bits
    }

    if (check_profile_idc(1) || check_profile_idc(2) || check_profile_idc(3) ||
        check_profile_idc(4) || check_profile_idc(5) || check_profile_idc(9) ||
        check_profile_idc(11)) {
      aDest.general_ptl.inbld_flag = aBr.ReadBit();
    } else {
      aBr.ReadBit();  // general_reserved_zero_bit
    }
  }

  aDest.general_ptl.level_idc = aBr.ReadBits(8);
  for (int i = 0; i < aMaxNumSubLayers - 1; i++) {
    aDest.sub_layer_profile_present_flag[i] = aBr.ReadBit();
    aDest.sub_layer_level_present_flag[i] = aBr.ReadBit();
  }
  if (aMaxNumSubLayers > 1) {
    for (int i = aMaxNumSubLayers - 1; i < 8; i++) {
      aBr.ReadBits(2);  // reserved_zero_2bits
    }
  }
  for (int i = 0; i < aMaxNumSubLayers - 1; i++) {
    if (aDest.sub_layer_profile_present_flag[i]) {
      aDest.sub_layer_ptl[i].profile_space = aBr.ReadBits(2);
      aDest.sub_layer_ptl[i].tier_flag = aBr.ReadBit();
      aDest.sub_layer_ptl[i].profile_idc = aBr.ReadBits(5);
      for (int j = 0; j < 32; j++) {
        aDest.sub_layer_ptl[i].profile_compatibility_flag[j] = aBr.ReadBit();
      }
      aDest.sub_layer_ptl[i].progressive_source_flag = aBr.ReadBit();
      aDest.sub_layer_ptl[i].interlaced_source_flag = aBr.ReadBit();
      aDest.sub_layer_ptl[i].non_packed_constraint_flag = aBr.ReadBit();
      aDest.sub_layer_ptl[i].frame_only_constraint_flag = aBr.ReadBit();

      auto check_sub_layer_profile_idc = [&aDest, i](int idc) {
        return aDest.sub_layer_ptl[i].profile_idc == idc ||
               aDest.sub_layer_ptl[i].profile_compatibility_flag[idc];
      };

      if (check_sub_layer_profile_idc(4) || check_sub_layer_profile_idc(5) ||
          check_sub_layer_profile_idc(6) || check_sub_layer_profile_idc(7) ||
          check_sub_layer_profile_idc(8) || check_sub_layer_profile_idc(9) ||
          check_sub_layer_profile_idc(10) || check_sub_layer_profile_idc(11)) {
        aDest.sub_layer_ptl[i].max_12bit_constraint_flag = aBr.ReadBit();
        aDest.sub_layer_ptl[i].max_10bit_constraint_flag = aBr.ReadBit();
        aDest.sub_layer_ptl[i].max_8bit_constraint_flag = aBr.ReadBit();
        aDest.sub_layer_ptl[i].max_422chroma_constraint_flag = aBr.ReadBit();
        aDest.sub_layer_ptl[i].max_420chroma_constraint_flag = aBr.ReadBit();
        aDest.sub_layer_ptl[i].max_monochrome_constraint_flag = aBr.ReadBit();
        aDest.sub_layer_ptl[i].intra_constraint_flag = aBr.ReadBit();
        aDest.sub_layer_ptl[i].one_picture_only_constraint_flag = aBr.ReadBit();
        aDest.sub_layer_ptl[i].lower_bit_rate_constraint_flag = aBr.ReadBit();
        if (check_sub_layer_profile_idc(5) || check_sub_layer_profile_idc(9) ||
            check_sub_layer_profile_idc(10) ||
            check_sub_layer_profile_idc(11)) {
          aDest.sub_layer_ptl[i].max_14bit_constraint_flag = aBr.ReadBit();
          aBr.ReadBits(33);  // sub_layer_reserved_zero_33bits
        } else {
          aBr.ReadBits(34);  // sub_layer_reserved_zero_34bits
        }
      } else if (check_sub_layer_profile_idc(2)) {
        aBr.ReadBits(7);  // sub_layer_reserved_zero_7bits
        aDest.sub_layer_ptl[i].one_picture_only_constraint_flag = aBr.ReadBit();
        aBr.ReadBits(35);  // sub_layer_reserved_zero_35bits
      } else {
        aBr.ReadBits(43);  // sub_layer_reserved_zero_43bits
      }
      if (check_sub_layer_profile_idc(1) || check_sub_layer_profile_idc(2) ||
          check_sub_layer_profile_idc(3) || check_sub_layer_profile_idc(4) ||
          check_sub_layer_profile_idc(5) || check_sub_layer_profile_idc(9) ||
          check_sub_layer_profile_idc(11)) {
        aDest.sub_layer_ptl[i].inbld_flag = aBr.ReadBit();
      } else {
        aBr.ReadBit();  // sub_layer_reserved_zero_bit
      }
    }
    if (aDest.sub_layer_level_present_flag[i]) {
      aDest.sub_layer_ptl[i].level_idc = aBr.ReadBits(8);
    }
  }
  return true;
}

bool B2GH265::ScalingList::Get(int aSizeId, int aMatrixId, Span<uint8_t>* aSl,
                               int16_t** aSlDc) {
  int index = (aSizeId == 3) ? aMatrixId / 3 : aMatrixId;
  switch (aSizeId) {
    case 0:  // 4x4
      *aSl = Span<uint8_t>(scaling_list_4x4[index]);
      return true;

    case 1:  // 8x8
      *aSl = Span<uint8_t>(scaling_list_8x8[index]);
      return true;

    case 2:  // 16x16
      *aSl = Span<uint8_t>(scaling_list_16x16[index]);
      *aSlDc = &scaling_list_dc_coef_16x16[index];
      return true;

    case 3:  // 32x32
      *aSl = Span<uint8_t>(scaling_list_32x32[index]);
      *aSlDc = &scaling_list_dc_coef_32x32[index];
      return true;

    default:
      return false;
  }
}

bool B2GH265::ScalingList::UseDefault(int aSizeId, int aMatrixId) {
  static const uint8_t sDefaultScalingList0[16] = {
      16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16};

  static const uint8_t sDefaultScalingList1[64] = {
      16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 17, 16, 17, 16, 17, 18,
      17, 18, 18, 17, 18, 21, 19, 20, 21, 20, 19, 21, 24, 22, 22, 24,
      24, 22, 22, 24, 25, 25, 27, 30, 27, 25, 25, 29, 31, 35, 35, 31,
      29, 36, 41, 44, 41, 36, 47, 54, 54, 47, 65, 70, 65, 88, 88, 115};

  static const uint8_t sDefaultScalingList2[64] = {
      16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 17, 17, 17, 17, 17, 18,
      18, 18, 18, 18, 18, 20, 20, 20, 20, 20, 20, 20, 24, 24, 24, 24,
      24, 24, 24, 24, 25, 25, 25, 25, 25, 25, 25, 28, 28, 28, 28, 28,
      28, 33, 33, 33, 33, 33, 41, 41, 41, 41, 54, 54, 54, 71, 71, 91};

  Span<uint8_t> sl;
  int16_t* slDc = nullptr;
  CHECK(Get(aSizeId, aMatrixId, &sl, &slDc));

  Span<const uint8_t> defaultSl;
  switch (aSizeId) {
    case 0:  // 4x4
      defaultSl = Span<const uint8_t>(sDefaultScalingList0);
      break;

    case 1:  // 8x8
    case 2:  // 16x16
    case 3:  // 32x32
      if (aMatrixId <= 2) {
        defaultSl = Span<const uint8_t>(sDefaultScalingList1);
      } else {
        defaultSl = Span<const uint8_t>(sDefaultScalingList2);
      }
      break;

    default:
      return false;
  }

  MOZ_ASSERT(sl.size() == defaultSl.size());
  std::copy(defaultSl.begin(), defaultSl.end(), sl.begin());

  if (aSizeId > 1) {
    MOZ_ASSERT(slDc);
    *slDc = 16;
  }
  return true;
}

bool B2GH265::ScalingList::UseReference(int aSizeId, int aMatrixId,
                                        int aRefMatrixId) {
  Span<uint8_t> sl;
  int16_t* slDc = nullptr;
  CHECK(Get(aSizeId, aMatrixId, &sl, &slDc));

  Span<uint8_t> refSl;
  int16_t* refSlDc = nullptr;
  CHECK(Get(aSizeId, aRefMatrixId, &refSl, &refSlDc));

  MOZ_ASSERT(sl.size() == refSl.size());
  std::copy(refSl.begin(), refSl.end(), sl.begin());

  if (aSizeId > 1) {
    MOZ_ASSERT(slDc && refSlDc);
    *slDc = *refSlDc;
  }
  return true;
}

/* static */
bool B2GH265::scaling_list_data(BitReader& aBr, ScalingList& aDest) {
  for (int sizeId = 0; sizeId < 4; sizeId++) {
    for (int matrixId = 0; matrixId < 6; matrixId += (sizeId == 3) ? 3 : 1) {
      bool scaling_list_pred_mode_flag = aBr.ReadBit();
      if (scaling_list_pred_mode_flag) {
        uint32_t scaling_list_pred_matrix_id_delta = aBr.ReadUE();
        CHECK_RANGE(scaling_list_pred_matrix_id_delta, 0,
                    (sizeId == 3) ? matrixId / 3 : matrixId);

        if (!scaling_list_pred_matrix_id_delta) {
          CHECK(aDest.UseDefault(sizeId, matrixId));
        } else {
          int refMatrixId = matrixId - scaling_list_pred_matrix_id_delta *
                                           (sizeId == 3 ? 3 : 1);
          CHECK(aDest.UseReference(sizeId, matrixId, refMatrixId));
        }
      } else {
        Span<uint8_t> sl;
        int16_t* slDc = nullptr;
        CHECK(aDest.Get(sizeId, matrixId, &sl, &slDc));

        int32_t nextCoef = 8;
        if (sizeId > 1) {
          MOZ_ASSERT(slDc);
          int32_t scaling_list_dc_coef = aBr.ReadSE() + 8;
          CHECK_RANGE(scaling_list_dc_coef, 1, 255);
          nextCoef = scaling_list_dc_coef;
          *slDc = nextCoef;
        }

        for (int i = 0; i < sl.size(); i++) {
          int32_t scaling_list_delta_coef = aBr.ReadSE();
          CHECK_RANGE(scaling_list_delta_coef, -128, 127);
          nextCoef = (nextCoef + scaling_list_delta_coef) & 0xff;
          sl[i] = nextCoef;
        }
      }
    }
  }
  return true;
}

/* static */
bool B2GH265::st_ref_pic_set(BitReader& aBr, ShortTermRPS& aStRps,
                             SPSData& aSps, int aStRpsIdx) {
  bool inter_ref_pic_set_prediction_flag = false;
  if (aStRpsIdx != 0) {
    inter_ref_pic_set_prediction_flag = aBr.ReadBit();
  }
  if (inter_ref_pic_set_prediction_flag) {
    int delta_idx = 1;
    if (aStRpsIdx == aSps.num_short_term_ref_pic_sets) {
      delta_idx = aBr.ReadUE() + 1;
      CHECK_RANGE(delta_idx, 1, aStRpsIdx);
    }

    int delta_rps_sign = aBr.ReadBit();
    int abs_delta_rps = aBr.ReadUE() + 1;
    CHECK_RANGE(abs_delta_rps, 1, 32768);

    int refRpsIdx = aStRpsIdx - delta_idx;
    int deltaRps = (1 - 2 * delta_rps_sign) * abs_delta_rps;

    auto& refRps = aSps.st_rps[refRpsIdx];
    bool used_by_curr_pic_flag[16] = {0};
    bool use_delta_flag[16] = {0};

    for (int j = 0; j <= refRps.NumDeltaPocs; j++) {
      used_by_curr_pic_flag[j] = aBr.ReadBit();
      if (!used_by_curr_pic_flag[j]) use_delta_flag[j] = aBr.ReadBit();
    }

    /* calculate NumNegativePics, DeltaPocS0 and UsedByCurrPicS0 */
    int i = 0;
    for (int j = refRps.NumPositivePics - 1; j >= 0; j--) {
      int dPoc = refRps.DeltaPocS1[j] + deltaRps;
      if (dPoc < 0 && use_delta_flag[refRps.NumNegativePics + j]) {
        aStRps.DeltaPocS0[i] = dPoc;
        aStRps.UsedByCurrPicS0[i++] =
            used_by_curr_pic_flag[refRps.NumNegativePics + j];
      }
    }
    if (deltaRps < 0 && use_delta_flag[refRps.NumDeltaPocs]) {
      aStRps.DeltaPocS0[i] = deltaRps;
      aStRps.UsedByCurrPicS0[i++] = used_by_curr_pic_flag[refRps.NumDeltaPocs];
    }
    for (int j = 0; j < refRps.NumNegativePics; j++) {
      int dPoc = refRps.DeltaPocS0[j] + deltaRps;
      if (dPoc < 0 && use_delta_flag[j]) {
        aStRps.DeltaPocS0[i] = dPoc;
        aStRps.UsedByCurrPicS0[i++] = used_by_curr_pic_flag[j];
      }
    }
    aStRps.NumNegativePics = i;

    /* calculate NumPositivePics, DeltaPocS1 and UsedByCurrPicS1 */
    i = 0;
    for (int j = refRps.NumNegativePics - 1; j >= 0; j--) {
      int dPoc = refRps.DeltaPocS0[j] + deltaRps;
      if (dPoc > 0 && use_delta_flag[j]) {
        aStRps.DeltaPocS1[i] = dPoc;
        aStRps.UsedByCurrPicS1[i++] = used_by_curr_pic_flag[j];
      }
    }
    if (deltaRps > 0 && use_delta_flag[refRps.NumDeltaPocs]) {
      aStRps.DeltaPocS1[i] = deltaRps;
      aStRps.UsedByCurrPicS1[i++] = used_by_curr_pic_flag[refRps.NumDeltaPocs];
    }
    for (int j = 0; j < refRps.NumPositivePics; j++) {
      int dPoc = refRps.DeltaPocS1[j] + deltaRps;
      if (dPoc > 0 && use_delta_flag[refRps.NumNegativePics + j]) {
        aStRps.DeltaPocS1[i] = dPoc;
        aStRps.UsedByCurrPicS1[i++] =
            used_by_curr_pic_flag[refRps.NumNegativePics + j];
      }
    }
    aStRps.NumPositivePics = i;

  } else {
    aStRps.NumNegativePics = aBr.ReadUE();  // num_negative_pics
    aStRps.NumPositivePics = aBr.ReadUE();  // num_positive_pics
    CHECK_RANGE(aStRps.NumNegativePics, 0,
                aSps.sps_max_dec_pic_buffering[aSps.sps_max_sub_layers - 1]);
    CHECK_RANGE(aStRps.NumPositivePics, 0,
                aSps.sps_max_dec_pic_buffering[aSps.sps_max_sub_layers - 1]);

    for (int i = 0; i < aStRps.NumNegativePics; i++) {
      int delta_poc_s0 = aBr.ReadUE() + 1;
      CHECK_RANGE(delta_poc_s0, 1, 32768);
      aStRps.UsedByCurrPicS0[i] = aBr.ReadBit();  // used_by_curr_pic_s0_flag

      if (i == 0) {
        aStRps.DeltaPocS0[i] = -delta_poc_s0;
      } else {
        aStRps.DeltaPocS0[i] = aStRps.DeltaPocS0[i - 1] - delta_poc_s0;
      }
    }

    for (int j = 0; j < aStRps.NumPositivePics; j++) {
      int delta_poc_s1 = aBr.ReadUE() + 1;
      CHECK_RANGE(delta_poc_s1, 1, 32768);
      aStRps.UsedByCurrPicS1[j] = aBr.ReadBit();  // used_by_curr_pic_s1_flag

      if (j == 0) {
        aStRps.DeltaPocS1[j] = delta_poc_s1;
      } else {
        aStRps.DeltaPocS1[j] = aStRps.DeltaPocS1[j - 1] + delta_poc_s1;
      }
    }
  }

  aStRps.NumDeltaPocs = aStRps.NumPositivePics + aStRps.NumNegativePics;
  return true;
}

/* static */
bool B2GH265::vui_parameters(BitReader& aBr, VUIData& aDest,
                             uint8_t aMaxNumSubLayers) {
  aDest.aspect_ratio_info_present_flag = aBr.ReadBit();
  if (aDest.aspect_ratio_info_present_flag) {
    aDest.aspect_ratio_idc = aBr.ReadBits(8);
    aDest.sar_width = aDest.sar_height = 0;

    switch (aDest.aspect_ratio_idc) {
      case 0:
        // Unspecified
        break;
      case 1:
        /*
          1:1
         7680x4320 16:9 frame without horizontal overscan
         3840x2160 16:9 frame without horizontal overscan
         1280x720 16:9 frame without horizontal overscan
         1920x1080 16:9 frame without horizontal overscan (cropped from
                        1920x1088)
         640x480 4:3 frame without horizontal overscan
         */
        aDest.sample_ratio = 1.0f;
        break;
      case 2:
        /*
          12:11
         720x576 4:3 frame with horizontal overscan
         352x288 4:3 frame without horizontal overscan
         */
        aDest.sample_ratio = 12.0 / 11.0;
        break;
      case 3:
        /*
          10:11
         720x480 4:3 frame with horizontal overscan
         352x240 4:3 frame without horizontal overscan
         */
        aDest.sample_ratio = 10.0 / 11.0;
        break;
      case 4:
        /*
          16:11
         720x576 16:9 frame with horizontal overscan
         528x576 4:3 frame without horizontal overscan
         */
        aDest.sample_ratio = 16.0 / 11.0;
        break;
      case 5:
        /*
          40:33
         720x480 16:9 frame with horizontal overscan
         528x480 4:3 frame without horizontal overscan
         */
        aDest.sample_ratio = 40.0 / 33.0;
        break;
      case 6:
        /*
          24:11
         352x576 4:3 frame without horizontal overscan
         480x576 16:9 frame with horizontal overscan
         */
        aDest.sample_ratio = 24.0 / 11.0;
        break;
      case 7:
        /*
          20:11
         352x480 4:3 frame without horizontal overscan
         480x480 16:9 frame with horizontal overscan
         */
        aDest.sample_ratio = 20.0 / 11.0;
        break;
      case 8:
        /*
          32:11
         352x576 16:9 frame without horizontal overscan
         */
        aDest.sample_ratio = 32.0 / 11.0;
        break;
      case 9:
        /*
          80:33
         352x480 16:9 frame without horizontal overscan
         */
        aDest.sample_ratio = 80.0 / 33.0;
        break;
      case 10:
        /*
          18:11
         480x576 4:3 frame with horizontal overscan
         */
        aDest.sample_ratio = 18.0 / 11.0;
        break;
      case 11:
        /*
          15:11
         480x480 4:3 frame with horizontal overscan
         */
        aDest.sample_ratio = 15.0 / 11.0;
        break;
      case 12:
        /*
          64:33
         528x576 16:9 frame with horizontal overscan
         */
        aDest.sample_ratio = 64.0 / 33.0;
        break;
      case 13:
        /*
          160:99
         528x480 16:9 frame without horizontal overscan
         */
        aDest.sample_ratio = 160.0 / 99.0;
        break;
      case 14:
        /*
          4:3
         1440x1080 16:9 frame without horizontal overscan
         */
        aDest.sample_ratio = 4.0 / 3.0;
        break;
      case 15:
        /*
          3:2
         1280x1080 16:9 frame without horizontal overscan
         */
        aDest.sample_ratio = 3.2 / 2.0;
        break;
      case 16:
        /*
          2:1
         960x1080 16:9 frame without horizontal overscan
         */
        aDest.sample_ratio = 2.0 / 1.0;
        break;
      case 255:
        /* EXTENDED_SAR */
        aDest.sar_width = aBr.ReadBits(16);
        aDest.sar_height = aBr.ReadBits(16);
        if (aDest.sar_width && aDest.sar_height) {
          aDest.sample_ratio = float(aDest.sar_width) / float(aDest.sar_height);
        }
        break;
      default:
        break;
    }
  }

  if (aBr.ReadBit()) {  // overscan_info_present_flag
    aDest.overscan_appropriate_flag = aBr.ReadBit();
  }

  if (aBr.ReadBit()) {  // video_signal_type_present_flag
    aDest.video_format = aBr.ReadBits(3);
    aDest.video_full_range_flag = aBr.ReadBit();
    aDest.colour_description_present_flag = aBr.ReadBit();
    if (aDest.colour_description_present_flag) {
      aDest.colour_primaries = aBr.ReadBits(8);
      aDest.transfer_characteristics = aBr.ReadBits(8);
      aDest.matrix_coefficients = aBr.ReadBits(8);
    }
  }

  aDest.chroma_loc_info_present_flag = aBr.ReadBit();
  if (aDest.chroma_loc_info_present_flag) {
    aDest.chroma_sample_loc_type_top_field = aBr.ReadUE();
    aDest.chroma_sample_loc_type_bottom_field = aBr.ReadUE();
    CHECK_RANGE(aDest.chroma_sample_loc_type_top_field, 0, 5);
    CHECK_RANGE(aDest.chroma_sample_loc_type_bottom_field, 0, 5);
  }

  aDest.neutral_chroma_indication_flag = aBr.ReadBit();
  aDest.field_seq_flag = aBr.ReadBit();
  aDest.frame_field_info_present_flag = aBr.ReadBit();

  aDest.default_display_window_flag = aBr.ReadBit();
  if (aDest.default_display_window_flag) {
    aDest.def_disp_win_left_offset = aBr.ReadUE();
    aDest.def_disp_win_right_offset = aBr.ReadUE();
    aDest.def_disp_win_top_offset = aBr.ReadUE();
    aDest.def_disp_win_bottom_offset = aBr.ReadUE();
  }

  aDest.timing_info_present_flag = aBr.ReadBit();
  if (aDest.timing_info_present_flag) {
    aDest.num_units_in_tick = aBr.ReadBits(32);
    aDest.time_scale = aBr.ReadBits(32);

    aDest.poc_proportional_to_timing_flag = aBr.ReadBit();
    if (aDest.poc_proportional_to_timing_flag) {
      aDest.num_ticks_poc_diff_one = aBr.ReadUE() + 1;
      CHECK_RANGE(aDest.num_ticks_poc_diff_one, 1, UINT32_MAX);
    }

    aDest.hrd_parameters_present_flag = aBr.ReadBit();
    if (aDest.hrd_parameters_present_flag) {
      hrd_parameters(aBr, 1, aMaxNumSubLayers);
    }
  }

  bool bitstream_restriction_flag = aBr.ReadBit();
  if (bitstream_restriction_flag) {
    aDest.tiles_fixed_structure_flag = aBr.ReadBit();
    aDest.motion_vectors_over_pic_boundaries_flag = aBr.ReadBit();
    aDest.restricted_ref_pic_lists_flag = aBr.ReadBit();
    aDest.min_spatial_segmentation_idc = aBr.ReadUE();
    aDest.max_bytes_per_pic_denom = aBr.ReadUE();
    aDest.max_bits_per_min_cu_denom = aBr.ReadUE();
    aDest.log2_max_mv_length_horizontal = aBr.ReadUE();
    aDest.log2_max_mv_length_vertical = aBr.ReadUE();
    CHECK_RANGE(aDest.min_spatial_segmentation_idc, 0, 4095);
    CHECK_RANGE(aDest.max_bytes_per_pic_denom, 0, 16);
    CHECK_RANGE(aDest.max_bits_per_min_cu_denom, 0, 16);
    CHECK_RANGE(aDest.log2_max_mv_length_horizontal, 0, 15);
    CHECK_RANGE(aDest.log2_max_mv_length_vertical, 0, 15);
  }
  return true;
}

/* static */
bool B2GH265::hrd_parameters(BitReader& aBr, bool aCommonInfPresentFlag,
                             uint8_t aMaxNumSubLayers) {
  bool nal_hrd_parameters_present_flag = false;
  bool vcl_hrd_parameters_present_flag = false;
  bool sub_pic_hrd_params_present_flag = false;
  if (aCommonInfPresentFlag) {
    nal_hrd_parameters_present_flag = aBr.ReadBit();
    vcl_hrd_parameters_present_flag = aBr.ReadBit();
    if (nal_hrd_parameters_present_flag || vcl_hrd_parameters_present_flag) {
      bool sub_pic_hrd_params_present_flag = aBr.ReadBit();
      if (sub_pic_hrd_params_present_flag) {
        aBr.ReadBits(8);  // tick_divisor_minus2
        aBr.ReadBits(5);  // du_cpb_removal_delay_increment_length_minus1
        aBr.ReadBit();    // sub_pic_cpb_params_in_pic_timing_sei_flag
        aBr.ReadBits(5);  // dpb_output_delay_du_length_minus1
      }
      aBr.ReadBits(4);  // bit_rate_scale
      aBr.ReadBits(4);  // cpb_size_scale
      if (sub_pic_hrd_params_present_flag) {
        aBr.ReadBits(4);  // cpb_size_du_scale
      }
      aBr.ReadBits(5);  // initial_cpb_removal_delay_length_minus1
      aBr.ReadBits(5);  // au_cpb_removal_delay_length_minus1
      aBr.ReadBits(5);  // dpb_output_delay_length_minus1
    }
  }

  for (int i = 0; i < aMaxNumSubLayers; i++) {
    bool fixed_pic_rate_general_flag = aBr.ReadBit();
    bool fixed_pic_rate_within_cvs_flag = true;
    if (!fixed_pic_rate_general_flag) {
      fixed_pic_rate_within_cvs_flag = aBr.ReadBit();
    }
    bool low_delay_hrd_flag = false;
    if (fixed_pic_rate_within_cvs_flag) {
      aBr.ReadUE();  // elemental_duration_in_tc_minus1
    } else {
      low_delay_hrd_flag = aBr.ReadBit();
    }
    uint32_t cpb_cnt = 1;
    if (!low_delay_hrd_flag) {
      cpb_cnt = aBr.ReadUE() + 1;  // cpb_cnt_minus1
    }
    if (nal_hrd_parameters_present_flag) {
      sub_layer_hrd_parameters(aBr, cpb_cnt, sub_pic_hrd_params_present_flag);
    }
    if (vcl_hrd_parameters_present_flag) {
      sub_layer_hrd_parameters(aBr, cpb_cnt, sub_pic_hrd_params_present_flag);
    }
  }
  return true;
}

/* static */
bool B2GH265::sub_layer_hrd_parameters(BitReader& aBr, uint32_t aCpbCnt,
                                       bool aSubPicHRDParamsPresentFlag) {
  for (int i = 0; i < aCpbCnt; i++) {
    aBr.ReadUE();  // bit_rate_value_minus1
    aBr.ReadUE();  // cpb_size_value_minus1
    if (aSubPicHRDParamsPresentFlag) {
      aBr.ReadUE();  // cpb_size_du_value_minus1
      aBr.ReadUE();  // bit_rate_du_value_minus1
    }
    aBr.ReadBit();  // cbr_flag
  }
  return true;
}

/* static */
bool B2GH265::HasParamSets(const MediaByteBuffer* aExtraData) {
  if (!aExtraData || aExtraData->IsEmpty()) {
    return false;
  }

  BufferReader reader(aExtraData);
  if (!reader.Read(22)) {
    return false;
  }
  auto numOfArrays = reader.ReadU8().unwrapOr(0);
  return numOfArrays > 0;
}

/* static */
already_AddRefed<MediaByteBuffer> B2GH265::ExtractExtraData(
    const MediaRawData* aSample) {
  MOZ_ASSERT(H265AnnexB::IsHVCC(aSample));

  size_t sampleSize = aSample->Size();
  if (aSample->mCrypto.IsEncrypted()) {
    // The content is encrypted, we can only parse the non-encrypted data.
    MOZ_ASSERT(aSample->mCrypto.mPlainSizes.Length() > 0);
    if (aSample->mCrypto.mPlainSizes.Length() == 0 ||
        aSample->mCrypto.mPlainSizes[0] > sampleSize) {
      // This is invalid content.
      return nullptr;
    }
    sampleSize = aSample->mCrypto.mPlainSizes[0];
  }

  // Store content of VPS, SPS, PPS
  nsTArray<uint8_t> vps, sps, pps;
  ByteWriter<BigEndian> vpsw(vps), spsw(sps), ppsw(pps);
  int numVps = 0, numSps = 0, numPps = 0;

  RefPtr<MediaByteBuffer> extradata = new MediaByteBuffer;
  BufferReader reader(aSample->Data(), sampleSize);

  int nalLenSize = ((*aSample->mExtraData)[21] & 3) + 1;

  uint8_t chromaFormatIdc, bitDepthLumaMinus8, bitDepthChromaMinus8;

  // Find VPS, SPS and PPS NALUs in HVCC data
  while (reader.Remaining() > nalLenSize) {
    uint32_t nalLen = ReadNalLen(reader, nalLenSize);
    const uint8_t* p = reader.Read(nalLen);
    if (!p) {
      // The read failed, but we may already have some VPS + SPS + PPS data so
      // break out of reading and process what we have, if any.
      break;
    }
    uint8_t nalType = (*p >> 1) & 0x3f;

    if (nalType == H265_NAL_VPS) {
      RefPtr<MediaByteBuffer> rbsp = DecodeNALUnit(p, nalLen);
      VPSData data;
      if (!DecodeVPS(rbsp, data)) {
        continue;
      }

      numVps++;
      if (!vpsw.WriteU16(nalLen) || !vpsw.Write(p, nalLen)) {
        return extradata.forget();
      }
    } else if (nalType == H265_NAL_SPS) {
      RefPtr<MediaByteBuffer> rbsp = DecodeNALUnit(p, nalLen);
      SPSData data;
      if (!DecodeSPS(rbsp, data)) {
        continue;
      }

      chromaFormatIdc = data.chroma_format_idc;
      bitDepthLumaMinus8 = data.bit_depth_luma - 8;
      bitDepthChromaMinus8 = data.bit_depth_chroma - 8;

      numSps++;
      if (!spsw.WriteU16(nalLen) || !spsw.Write(p, nalLen)) {
        return extradata.forget();
      }
    } else if (nalType == H265_NAL_PPS) {
      numPps++;
      if (!ppsw.WriteU16(nalLen) || !ppsw.Write(p, nalLen)) {
        return extradata.forget();
      }
    }
  }

  if (numVps == 0 || numSps == 0) {
    return nullptr;
  }

  const int vpsGeneralPtlStart = 4;
  const int vpsGeneralPtlEnd = vpsGeneralPtlStart + 12;
  if (vps.Length() < vpsGeneralPtlEnd) {
    return nullptr;
  }

  int numOfArrays = !!numVps + !!numSps + !!numPps;

  // TODO: create HVCC extra data
  extradata->AppendElement(1);  // version
  for (int i = vpsGeneralPtlStart; i < vpsGeneralPtlEnd; i++) {
    extradata->AppendElement(vps[i]);
  }
  extradata->AppendElement(0xf0);  // FIXME: min_spatial_segmentation_idc
  extradata->AppendElement(0);
  extradata->AppendElement(0xfc);  // FIXME: derive parallelismType
  extradata->AppendElement(0xfc | chromaFormatIdc);
  extradata->AppendElement(0xf8 | bitDepthLumaMinus8);
  extradata->AppendElement(0xf8 | bitDepthChromaMinus8);
  extradata->AppendElement(0);  // FIXME: derive avgFrameRate
  extradata->AppendElement(0);
  extradata->AppendElement(nalLenSize - 1);
  extradata->AppendElement(numOfArrays);

  auto appendNALArray = [extradata](uint8_t aNALType, int aNumNAL,
                                    nsTArray<uint8_t>& aArray) {
    if (aNumNAL == 0) {
      return;
    }

    extradata->AppendElement(aNALType | 0x80);
    extradata->AppendElement((aNumNAL >> 8) & 0xff);
    extradata->AppendElement(aNumNAL & 0xff);
    extradata->AppendElements(aArray.Elements(), aArray.Length());
  };

  appendNALArray(H265_NAL_VPS, numVps, vps);
  appendNALArray(H265_NAL_SPS, numSps, sps);
  appendNALArray(H265_NAL_PPS, numPps, pps);
  return extradata.forget();
}

#undef READUE
#undef READSE

}  // namespace mozilla
