#include "h264_slice_header.h"

#include "bit_reader.h"

#include <string.h>

int vsa_h264_parse_slice_header(const uint8_t*         rbsp,
                                size_t                 size,
                                uint8_t                nal_unit_type,
                                const vsa_h264_sps*    sps,
                                vsa_h264_slice_header* out) {
    if (rbsp == NULL || sps == NULL || out == NULL || size == 0) {
        return -1;
    }

    memset(out, 0, sizeof(*out));

    vsa_bit_reader br;
    vsa_br_init(&br, rbsp, size);

    out->first_mb_in_slice    = vsa_br_read_ue(&br);
    out->slice_type           = vsa_br_read_ue(&br);
    out->pic_parameter_set_id = vsa_br_read_ue(&br);

    if (sps->separate_colour_plane_flag) {
        out->colour_plane_id     = vsa_br_read_bits(&br, 2);
        out->has_colour_plane_id = 1;
    }

    /* frame_num is u(log2_max_frame_num_minus4 + 4). */
    {
        const int log2_max_frame_num =
            (int)sps->log2_max_frame_num_minus4 + 4;
        out->frame_num = vsa_br_read_bits(&br, log2_max_frame_num);
    }

    if (!sps->frame_mbs_only_flag) {
        out->field_pic_flag = (uint8_t)vsa_br_read_u1(&br);
        if (out->field_pic_flag) {
            out->bottom_field_flag = (uint8_t)vsa_br_read_u1(&br);
        }
    }

    /* IDR pictures (NAL type 5) carry an idr_pic_id field. */
    if (nal_unit_type == 5) {
        out->idr_pic_id     = vsa_br_read_ue(&br);
        out->has_idr_pic_id = 1;
    }

    if (sps->pic_order_cnt_type == 0) {
        const int log2_max_poc_lsb =
            (int)sps->log2_max_pic_order_cnt_lsb_minus4 + 4;
        out->pic_order_cnt_lsb     = vsa_br_read_bits(&br, log2_max_poc_lsb);
        out->has_pic_order_cnt_lsb = 1;
        /* delta_pic_order_cnt_bottom is gated on a PPS field we do not
         * yet parse. Leaving it unread is safe because we stop here
         * and the caller never re-enters the bit stream. */
    }

    if (vsa_br_error(&br)) {
        return -2;
    }
    return 0;
}

char vsa_h264_slice_type_letter(uint32_t slice_type) {
    switch (slice_type % 5u) {
        case 0u: return 'P';
        case 1u: return 'B';
        case 2u: return 'I';
        case 3u: return 'S';   /* SP */
        case 4u: return 'i';   /* SI (lowercase to disambiguate) */
        default: return '?';
    }
}
