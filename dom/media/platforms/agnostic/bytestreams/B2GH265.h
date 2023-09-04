/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef B2GH265_H
#define B2GH265_H

#include "DecoderData.h"
#include "mozilla/gfx/Types.h"

namespace mozilla {
class BitReader;

// 7.4.3.1: vps_max_sub_layers_minus1 is in [0, 6].
#define HEVC_MAX_SUB_LAYERS 7

// 7.4.2.1: vps_video_parameter_set_id is u(4).
#define HEVC_MAX_VPS_COUNT 16

// 7.4.3.2.1: sps_seq_parameter_set_id is in [0, 15].
#define HEVC_MAX_SPS_COUNT 16

// 7.4.3.3.1: pps_pic_parameter_set_id is in [0, 63].
#define HEVC_MAX_PPS_COUNT 64

// A.4.2: MaxDpbSize is bounded above by 16.
#define HEVC_MAX_DPB_SIZE 16

// 7.4.3.2.1: num_short_term_ref_pic_sets is in [0, 64].
#define HEVC_MAX_SHORT_TERM_REF_PIC_SETS 64

// 7.4.3.2.1: num_long_term_ref_pics_sps is in [0, 32].
#define HEVC_MAX_LONG_TERM_REF_PICS 32

class B2GH265 {
 public:
  // NAL unit types
  enum NAL_TYPES {
    H265_NAL_VPS = 32,
    H265_NAL_SPS = 33,
    H265_NAL_PPS = 34,
    H265_NAL_PREFIX_SEI = 39,
    H265_NAL_SUFFIX_SEI = 40,
  };

  struct PTLData {
    struct Common {
      uint8_t profile_space;
      bool tier_flag;
      uint8_t profile_idc;
      bool profile_compatibility_flag[32];
      bool progressive_source_flag;
      bool interlaced_source_flag;
      bool non_packed_constraint_flag;
      bool frame_only_constraint_flag;
      bool max_12bit_constraint_flag;
      bool max_10bit_constraint_flag;
      bool max_8bit_constraint_flag;
      bool max_422chroma_constraint_flag;
      bool max_420chroma_constraint_flag;
      bool max_monochrome_constraint_flag;
      bool intra_constraint_flag;
      bool one_picture_only_constraint_flag;
      bool lower_bit_rate_constraint_flag;
      bool max_14bit_constraint_flag;
      bool inbld_flag;
      uint8_t level_idc;
    };

    Common general_ptl;
    Common sub_layer_ptl[HEVC_MAX_SUB_LAYERS];
    bool sub_layer_profile_present_flag[HEVC_MAX_SUB_LAYERS];
    bool sub_layer_level_present_flag[HEVC_MAX_SUB_LAYERS];
  };

  struct ScalingList {
    uint8_t scaling_list_4x4[6][16];
    uint8_t scaling_list_8x8[6][64];
    uint8_t scaling_list_16x16[6][64];
    uint8_t scaling_list_32x32[2][64];

    int16_t scaling_list_dc_coef_16x16[6];
    int16_t scaling_list_dc_coef_32x32[2];

    bool Get(int aSizeId, int aMatrixId, Span<uint8_t>* aSl, int16_t** aSlDc);

    bool UseDefault(int aSizeId, int aMatrixId);

    bool UseReference(int aSizeId, int aMatrixId, int aRefMatrixId);
  };

  struct ShortTermRPS {
    uint8_t NumDeltaPocs;
    uint8_t NumNegativePics;
    uint8_t NumPositivePics;
    bool UsedByCurrPicS0[16];
    bool UsedByCurrPicS1[16];
    int32_t DeltaPocS0[16];
    int32_t DeltaPocS1[16];
  };

  struct VUIData {
    bool aspect_ratio_info_present_flag;
    uint8_t aspect_ratio_idc;
    uint32_t sar_width;
    uint32_t sar_height;
    float sample_ratio;

    bool overscan_appropriate_flag;
    uint8_t video_format;
    bool video_full_range_flag;
    bool colour_description_present_flag;
    uint8_t colour_primaries;
    uint8_t transfer_characteristics;
    uint8_t matrix_coefficients;

    bool chroma_loc_info_present_flag;
    uint8_t chroma_sample_loc_type_top_field;
    uint8_t chroma_sample_loc_type_bottom_field;

    bool neutral_chroma_indication_flag;
    bool field_seq_flag;
    bool frame_field_info_present_flag;

    bool default_display_window_flag;
    uint32_t def_disp_win_left_offset;
    uint32_t def_disp_win_right_offset;
    uint32_t def_disp_win_top_offset;
    uint32_t def_disp_win_bottom_offset;

    bool timing_info_present_flag;
    uint32_t num_units_in_tick;
    uint32_t time_scale;

    bool poc_proportional_to_timing_flag;
    uint32_t num_ticks_poc_diff_one;

    bool hrd_parameters_present_flag;

    bool tiles_fixed_structure_flag;
    bool motion_vectors_over_pic_boundaries_flag;
    bool restricted_ref_pic_lists_flag;
    uint32_t min_spatial_segmentation_idc;
    uint32_t max_bytes_per_pic_denom;
    uint32_t max_bits_per_min_cu_denom;
    uint32_t log2_max_mv_length_horizontal;
    uint32_t log2_max_mv_length_vertical;
  };

  struct VPSData {
    uint8_t vps_video_parameter_set_id;
    bool vps_base_layer_internal_flag;
    bool vps_base_layer_available_flag;
    uint8_t vps_max_layers;
    uint8_t vps_max_sub_layers;
    bool vps_temporal_id_nesting_flag;

    PTLData ptl;
    bool vps_sub_layer_ordering_info_present_flag;
    uint32_t vps_max_dec_pic_buffering[HEVC_MAX_SUB_LAYERS];
    uint32_t vps_max_num_reorder_pics[HEVC_MAX_SUB_LAYERS];
    uint32_t vps_max_latency_increase[HEVC_MAX_SUB_LAYERS];
    uint8_t vps_max_layer_id;
    uint32_t vps_num_layer_sets;
    bool vps_timing_info_present_flag;
    uint32_t vps_num_units_in_tick;
    uint32_t vps_time_scale;
    bool vps_poc_proportional_to_timing_flag;
    uint32_t vps_num_ticks_poc_diff_one;
    uint32_t vps_num_hrd_parameters;

    VPSData();
  };

  struct SPSData {
    uint8_t sps_video_parameter_set_id;
    uint8_t sps_max_sub_layers;
    bool sps_temporal_id_nesting_flag;
    uint32_t sps_seq_parameter_set_id;
    uint32_t chroma_format_idc;
    bool separate_colour_plane_flag;

    int pic_width_in_luma_samples;
    int pic_height_in_luma_samples;

    bool conformance_window_flag;
    uint32_t conf_win_left_offset;
    uint32_t conf_win_right_offset;
    uint32_t conf_win_top_offset;
    uint32_t conf_win_bottom_offset;

    uint32_t bit_depth_luma;
    uint32_t bit_depth_chroma;
    uint32_t log2_max_pic_order_cnt_lsb;

    uint32_t sps_max_dec_pic_buffering[HEVC_MAX_SUB_LAYERS];
    uint32_t sps_max_num_reorder_pics[HEVC_MAX_SUB_LAYERS];
    uint32_t sps_max_latency_increase[HEVC_MAX_SUB_LAYERS];

    uint32_t log2_min_luma_coding_block_size;
    uint32_t log2_diff_max_min_luma_coding_block_size;
    uint32_t log2_min_luma_transform_block_size;
    uint32_t log2_diff_max_min_luma_transform_block_size;

    uint32_t max_transform_hierarchy_depth_inter;
    uint32_t max_transform_hierarchy_depth_intra;

    bool scaling_list_enable_flag;
    bool sps_scaling_list_data_present_flag;
    ScalingList scaling_list;

    bool amp_enabled_flag;
    bool sample_adaptive_offset_enabled_flag;
    bool pcm_enabled_flag;

    uint8_t pcm_sample_bit_depth_luma;
    uint8_t pcm_sample_bit_depth_chroma;
    uint32_t log2_min_pcm_luma_coding_block_size;
    uint32_t log2_diff_max_min_pcm_luma_coding_block_size;
    bool pcm_loop_filter_disabled_flag;

    uint32_t num_short_term_ref_pic_sets;
    ShortTermRPS st_rps[HEVC_MAX_SHORT_TERM_REF_PIC_SETS];
    bool long_term_ref_pics_present_flag;
    uint32_t num_long_term_ref_pics_sps;
    uint16_t lt_ref_pic_poc_lsb_sps[HEVC_MAX_LONG_TERM_REF_PICS];
    uint8_t used_by_curr_pic_lt_sps_flag[HEVC_MAX_LONG_TERM_REF_PICS];

    bool sps_temporal_mvp_enabled_flag;
    bool sps_strong_intra_smoothing_enable_flag;

    VUIData vui;
    PTLData ptl;

    SPSData();
    gfx::YUVColorSpace ColorSpace() const;
    gfx::ColorDepth ColorDepth() const;
  };

  // Check if out of band extradata contains a SPS NAL.
  static bool HasParamSets(const MediaByteBuffer* aExtraData);

  // Extract SPS and PPS NALs from aSample by looking into each NALs.
  // aSample must be in HVCC format.
  static already_AddRefed<MediaByteBuffer> ExtractExtraData(
      const MediaRawData* aSample);

  static bool DecodeVPSFromExtraData(const MediaByteBuffer* aExtraData,
                                     VPSData& aDest);

  static bool DecodeSPSFromExtraData(const MediaByteBuffer* aExtraData,
                                     SPSData& aDest);

  static bool DecodeVPS(const MediaByteBuffer* aVPS, VPSData& aDest);

  static bool DecodeSPS(const MediaByteBuffer* aSPS, SPSData& aDest);

  enum class FrameType {
    I_FRAME,
    OTHER,
    INVALID,
  };

  static FrameType GetFrameType(const MediaRawData* aSample);

 private:
  static bool DecodeNALUnitFromExtraData(
      const MediaByteBuffer* aExtraData, uint8_t aNALType,
      const std::function<bool(const uint8_t*, size_t)>& aNALDecoder);

  static already_AddRefed<MediaByteBuffer> DecodeNALUnit(const uint8_t* aNAL,
                                                         size_t aLength);

  static bool video_parameter_set_rbsp(BitReader& aBr, VPSData& aDest);

  static bool seq_parameter_set_rbsp(BitReader& aBr, SPSData& aDest);

  static bool profile_tier_level(BitReader& aBr, PTLData& aDest,
                                 bool aProfilePresentFlag,
                                 uint8_t aMaxNumSubLayers);

  static bool scaling_list_data(BitReader& aBr, ScalingList& aDest);

  static bool st_ref_pic_set(BitReader& aBr, ShortTermRPS& aStRps,
                             SPSData& aSps, int aStRpsIdx);

  static bool vui_parameters(BitReader& aBr, VUIData& aDest,
                             uint8_t aMaxNumSubLayers);
  // All data is ignored.
  static bool hrd_parameters(BitReader& aBr, bool aCommonInfPresentFlag,
                             uint8_t aMaxNumSubLayers);
  // All data is ignored.
  static bool sub_layer_hrd_parameters(BitReader& aBr, uint32_t aCpbCnt,
                                       bool aSubPicHRDParamsPresentFlag);
};

}  // namespace mozilla

#endif  // B2GH265_H
