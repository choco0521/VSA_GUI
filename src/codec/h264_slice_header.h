#ifndef VSA_CODEC_H264_SLICE_HEADER_H
#define VSA_CODEC_H264_SLICE_HEADER_H

#include <stddef.h>
#include <stdint.h>

#include "h264_sps.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Parsed H.264 slice header (ISO/IEC 14496-10 §7.3.3), trimmed to the
 * fields the VSA GUI surfaces in its frame-table view: slice type,
 * frame number, IDR id, picture order count LSB, and the field/frame
 * flags needed to interpret them.
 *
 * The fields after dec_ref_pic_marking — slice_qp_delta, deblock
 * params, ref_pic_list_modification, etc. — depend on PPS context
 * that we do not yet parse. They will be added in a follow-up that
 * also introduces vsa_h264_pps. For now the parser stops at the end
 * of the picture-order-count block and returns 0 if everything up to
 * that point decoded cleanly.
 */
typedef struct vsa_h264_slice_header {
    uint32_t first_mb_in_slice;
    uint32_t slice_type;             /* raw 0..9 — see Table 7-6      */
    uint32_t pic_parameter_set_id;
    uint32_t colour_plane_id;        /* only if separate_colour_plane */
    uint32_t frame_num;
    uint8_t  field_pic_flag;
    uint8_t  bottom_field_flag;
    uint32_t idr_pic_id;             /* only valid when IDR           */
    uint32_t pic_order_cnt_lsb;      /* only valid when poc_type==0   */

    uint8_t  has_colour_plane_id;
    uint8_t  has_idr_pic_id;
    uint8_t  has_pic_order_cnt_lsb;
} vsa_h264_slice_header;

/*
 * Parse a slice NAL's RBSP into `out`. The caller must:
 *   - strip the NAL header byte
 *   - run vsa_rbsp_unescape to remove 00 00 03 escapes
 *   - pass the active SPS via `sps`
 *   - pass the original NAL header's nal_unit_type (5 for IDR,
 *     1 for non-IDR, 19 for aux, 20 for ext) so the parser knows
 *     whether to read idr_pic_id.
 *
 * Returns 0 on success. Negative on parse error:
 *     -1  null inputs
 *     -2  bit reader hit EOF mid-parse
 */
int vsa_h264_parse_slice_header(const uint8_t*            rbsp,
                                size_t                    size,
                                uint8_t                   nal_unit_type,
                                const vsa_h264_sps*       sps,
                                vsa_h264_slice_header*    out);

/*
 * Convenience: returns the canonical letter for a slice_type field.
 * Maps slice_type % 5 → 'P','B','I','S','i' (for SI). Unknown → '?'.
 */
char vsa_h264_slice_type_letter(uint32_t slice_type);

#ifdef __cplusplus
}
#endif

#endif /* VSA_CODEC_H264_SLICE_HEADER_H */
