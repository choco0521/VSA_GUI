#include "h264_sps.h"

#include "bit_reader.h"

#include <string.h>

/* "High" profiles that carry the extended syntax starting with
 * chroma_format_idc. ISO/IEC 14496-10 §7.3.2.1.1. */
static int sps_profile_has_high_ext(uint8_t profile_idc) {
    switch (profile_idc) {
        case 100: /* High */
        case 110: /* High 10 */
        case 122: /* High 4:2:2 */
        case 244: /* High 4:4:4 Predictive */
        case  44: /* CAVLC 4:4:4 */
        case  83: /* Scalable Baseline */
        case  86: /* Scalable High */
        case 118: /* Multiview High */
        case 128: /* Stereo High */
        case 138: /* MFC High */
        case 139: /* MFC Depth High */
        case 134: /* MFC High extension */
        case 135: /* not-really-used but in the spec table */
            return 1;
        default:
            return 0;
    }
}

int vsa_h264_parse_sps(const uint8_t* rbsp, size_t size, vsa_h264_sps* out) {
    if (rbsp == NULL || out == NULL || size < 3) {
        return -2;
    }

    memset(out, 0, sizeof(*out));
    /* Spec-mandated defaults for fields that are absent outside the
     * high profiles. */
    out->chroma_format_idc = 1u;  /* 4:2:0 */

    vsa_bit_reader br;
    vsa_br_init(&br, rbsp, size);

    out->profile_idc = (uint8_t)vsa_br_read_bits(&br, 8);

    /* Six constraint_setN_flag values packed MSB-first followed by
     * two reserved zero bits. Read each flag individually so the
     * output struct mirrors the JM ldecod trace field names exactly,
     * which makes the Phase D diff straightforward. */
    out->constraint_set0_flag = (uint8_t)vsa_br_read_u1(&br);
    out->constraint_set1_flag = (uint8_t)vsa_br_read_u1(&br);
    out->constraint_set2_flag = (uint8_t)vsa_br_read_u1(&br);
    out->constraint_set3_flag = (uint8_t)vsa_br_read_u1(&br);
    out->constraint_set4_flag = (uint8_t)vsa_br_read_u1(&br);
    out->constraint_set5_flag = (uint8_t)vsa_br_read_u1(&br);
    (void)vsa_br_read_bits(&br, 2);  /* reserved_zero_2bits */

    out->level_idc = (uint8_t)vsa_br_read_bits(&br, 8);

    out->seq_parameter_set_id = vsa_br_read_ue(&br);

    if (sps_profile_has_high_ext(out->profile_idc)) {
        out->chroma_format_idc = vsa_br_read_ue(&br);
        if (out->chroma_format_idc == 3u) {
            out->separate_colour_plane_flag =
                (uint8_t)vsa_br_read_u1(&br);
        }
        out->bit_depth_luma_minus8            = vsa_br_read_ue(&br);
        out->bit_depth_chroma_minus8          = vsa_br_read_ue(&br);
        out->qpprime_y_zero_transform_bypass_flag =
            (uint8_t)vsa_br_read_u1(&br);
        out->seq_scaling_matrix_present_flag =
            (uint8_t)vsa_br_read_u1(&br);
        if (out->seq_scaling_matrix_present_flag) {
            /* Scaling lists are complex and not needed for dimension
             * extraction. Rather than silently mis-parse the tail of
             * the SPS we refuse the stream. */
            return -4;
        }
    }

    out->log2_max_frame_num_minus4 = vsa_br_read_ue(&br);
    out->pic_order_cnt_type        = vsa_br_read_ue(&br);

    if (out->pic_order_cnt_type == 0u) {
        out->log2_max_pic_order_cnt_lsb_minus4 = vsa_br_read_ue(&br);
    } else if (out->pic_order_cnt_type == 1u) {
        out->delta_pic_order_always_zero_flag =
            (uint8_t)vsa_br_read_u1(&br);
        out->offset_for_non_ref_pic            = vsa_br_read_se(&br);
        out->offset_for_top_to_bottom_field    = vsa_br_read_se(&br);
        out->num_ref_frames_in_pic_order_cnt_cycle = vsa_br_read_ue(&br);
        /* Skip the per-frame offsets — we don't yet use them. */
        for (uint32_t i = 0;
             i < out->num_ref_frames_in_pic_order_cnt_cycle;
             ++i) {
            (void)vsa_br_read_se(&br);
        }
    } else if (out->pic_order_cnt_type == 2u) {
        /* pic_order_cnt_type == 2: POC is implicitly derived from
         * frame_num (no extra syntax elements in the SPS). H.264
         * §7.4.2.1.1 — nothing to read here. */
    } else {
        /* 3..N are reserved and not defined by the spec. */
        return -3;
    }

    out->max_num_ref_frames                    = vsa_br_read_ue(&br);
    out->gaps_in_frame_num_value_allowed_flag =
        (uint8_t)vsa_br_read_u1(&br);
    out->pic_width_in_mbs_minus1               = vsa_br_read_ue(&br);
    out->pic_height_in_map_units_minus1        = vsa_br_read_ue(&br);
    out->frame_mbs_only_flag                   =
        (uint8_t)vsa_br_read_u1(&br);

    if (!out->frame_mbs_only_flag) {
        out->mb_adaptive_frame_field_flag =
            (uint8_t)vsa_br_read_u1(&br);
    }

    out->direct_8x8_inference_flag =
        (uint8_t)vsa_br_read_u1(&br);

    out->frame_cropping_flag = (uint8_t)vsa_br_read_u1(&br);
    if (out->frame_cropping_flag) {
        out->frame_crop_left_offset   = vsa_br_read_ue(&br);
        out->frame_crop_right_offset  = vsa_br_read_ue(&br);
        out->frame_crop_top_offset    = vsa_br_read_ue(&br);
        out->frame_crop_bottom_offset = vsa_br_read_ue(&br);
    }

    out->vui_parameters_present_flag = (uint8_t)vsa_br_read_u1(&br);
    /* Deliberately stop here: VUI parsing is not required for
     * picture dimensions, profile identification or reference count. */

    if (vsa_br_error(&br)) {
        return -5;
    }
    return 0;
}

int vsa_h264_sps_picture_size(const vsa_h264_sps* sps,
                              uint32_t*           width,
                              uint32_t*           height) {
    if (sps == NULL || width == NULL || height == NULL) {
        return -1;
    }

    /* SubWidthC / SubHeightC from H.264 Table 6-1. */
    uint32_t sub_w = 2, sub_h = 2;  /* 4:2:0 default */
    switch (sps->chroma_format_idc) {
        case 0: sub_w = 0; sub_h = 0; break;   /* monochrome   */
        case 1: sub_w = 2; sub_h = 2; break;   /* 4:2:0        */
        case 2: sub_w = 2; sub_h = 1; break;   /* 4:2:2        */
        case 3:
            if (sps->separate_colour_plane_flag == 0u) {
                sub_w = 1; sub_h = 1;          /* 4:4:4        */
            } else {
                sub_w = 0; sub_h = 0;          /* separated    */
            }
            break;
        default: return -2;
    }

    const uint32_t pic_width_in_mbs = sps->pic_width_in_mbs_minus1 + 1u;
    const uint32_t pic_height_in_map_units =
        sps->pic_height_in_map_units_minus1 + 1u;
    const uint32_t frame_height_in_mbs =
        (sps->frame_mbs_only_flag ? 1u : 2u) * pic_height_in_map_units;

    uint32_t coded_w = pic_width_in_mbs * 16u;
    uint32_t coded_h = frame_height_in_mbs * 16u;

    if (!sps->frame_cropping_flag) {
        *width  = coded_w;
        *height = coded_h;
        return 0;
    }

    uint32_t crop_x = sub_w;
    uint32_t crop_y = sub_h * (sps->frame_mbs_only_flag ? 1u : 2u);
    /* Monochrome chroma_format_idc == 0 degenerates: the spec
     * treats SubWidthC = SubHeightC = 1 in that case. */
    if (sps->chroma_format_idc == 0u) {
        crop_x = 1u;
        crop_y = (sps->frame_mbs_only_flag ? 1u : 2u);
    }

    const uint32_t crop_w = crop_x * (sps->frame_crop_left_offset +
                                      sps->frame_crop_right_offset);
    const uint32_t crop_h = crop_y * (sps->frame_crop_top_offset +
                                      sps->frame_crop_bottom_offset);

    if (crop_w >= coded_w || crop_h >= coded_h) {
        return -3;
    }

    *width  = coded_w - crop_w;
    *height = coded_h - crop_h;
    return 0;
}
