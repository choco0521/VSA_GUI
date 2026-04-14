#ifndef VSA_CODEC_H264_SPS_H
#define VSA_CODEC_H264_SPS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Parsed H.264 Sequence Parameter Set (ISO/IEC 14496-10 §7.3.2.1.1).
 *
 * This struct covers the fields the VSA GUI actually surfaces in its
 * Stream Info tree and those needed to compute picture dimensions.
 * Scaling matrices, VUI HRD parameters and extended-profile payloads
 * (SVC/MVC) are deliberately skipped: we record their *presence*
 * through a flag but never decode them. Callers that need richer
 * metadata should bail out on `vui_parameters_present_flag` or
 * `seq_scaling_matrix_present_flag` and fall back to a heavier parser.
 *
 * The integer fields store raw syntax-element values. "Friendly"
 * derived quantities (width in pixels, bit depth, etc.) are computed
 * by helper functions below so the struct stays a plain POD with
 * stable wire semantics.
 */
typedef struct vsa_h264_sps {
    uint8_t  profile_idc;
    uint8_t  constraint_set0_flag;
    uint8_t  constraint_set1_flag;
    uint8_t  constraint_set2_flag;
    uint8_t  constraint_set3_flag;
    uint8_t  constraint_set4_flag;
    uint8_t  constraint_set5_flag;
    uint8_t  level_idc;
    uint32_t seq_parameter_set_id;

    /* High-profile-only fields. Populated only when profile_idc is one
     * of {100,110,122,244,44,83,86,118,128,138,139,134,135}. The
     * defaults below match what the spec mandates for the baseline/
     * main/extended profiles where these fields are absent. */
    uint32_t chroma_format_idc;          /* default 1 (4:2:0) */
    uint8_t  separate_colour_plane_flag; /* default 0         */
    uint32_t bit_depth_luma_minus8;      /* default 0 → 8 bpp */
    uint32_t bit_depth_chroma_minus8;    /* default 0 → 8 bpp */
    uint8_t  qpprime_y_zero_transform_bypass_flag;
    uint8_t  seq_scaling_matrix_present_flag;

    uint32_t log2_max_frame_num_minus4;
    uint32_t pic_order_cnt_type;

    /* Valid when pic_order_cnt_type == 0. */
    uint32_t log2_max_pic_order_cnt_lsb_minus4;

    /* Valid when pic_order_cnt_type == 1. The ref-frame cycle is not
     * captured — only its length is, so callers can detect streams we
     * cannot yet fully decode. */
    uint8_t  delta_pic_order_always_zero_flag;
    int32_t  offset_for_non_ref_pic;
    int32_t  offset_for_top_to_bottom_field;
    uint32_t num_ref_frames_in_pic_order_cnt_cycle;

    uint32_t max_num_ref_frames;
    uint8_t  gaps_in_frame_num_value_allowed_flag;
    uint32_t pic_width_in_mbs_minus1;
    uint32_t pic_height_in_map_units_minus1;
    uint8_t  frame_mbs_only_flag;
    uint8_t  mb_adaptive_frame_field_flag;
    uint8_t  direct_8x8_inference_flag;

    uint8_t  frame_cropping_flag;
    uint32_t frame_crop_left_offset;
    uint32_t frame_crop_right_offset;
    uint32_t frame_crop_top_offset;
    uint32_t frame_crop_bottom_offset;

    uint8_t  vui_parameters_present_flag;
} vsa_h264_sps;

/*
 * Parse the RBSP (emulation-prevention bytes already removed) of an
 * SPS NAL. The NAL header byte must *not* be included in the input
 * — callers should strip it and call vsa_rbsp_unescape() first.
 *
 * Returns 0 on success and fills `*out`. Returns a negative value on
 * any parse error; `*out` is then left in an unspecified state.
 *
 * Unsupported features deliberately cause an error so we never
 * silently report wrong dimensions:
 *     -2  empty or truncated input
 *     -3  pic_order_cnt_type > 1
 *     -4  seq_scaling_matrix_present_flag set (not implemented)
 *     -5  bitstream ran out mid-parse (BitReader error)
 */
int vsa_h264_parse_sps(const uint8_t*  rbsp,
                       size_t          size,
                       vsa_h264_sps*   out);

/*
 * Compute the luma picture width / height in pixels after frame
 * cropping, honouring frame_mbs_only_flag and chroma_format_idc-
 * dependent SubHeight/SubWidth.
 *
 * Returns 0 on success and writes to *width / *height. Returns a
 * negative value if the SPS is internally inconsistent (e.g. a
 * crop offset wider than the coded picture).
 */
int vsa_h264_sps_picture_size(const vsa_h264_sps* sps,
                              uint32_t*           width,
                              uint32_t*           height);

#ifdef __cplusplus
}
#endif

#endif /* VSA_CODEC_H264_SPS_H */
