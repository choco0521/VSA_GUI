#include "annexb.h"

/* Returns the length of the start code (3 or 4) beginning at `p`, or 0
 * if there is no start code there. `remaining` is the number of bytes
 * still available from `p` onwards.
 *
 * The 4-byte form must be tested before the 3-byte form — otherwise a
 * `00 00 00 01` prefix would be reported as two overlapping start
 * codes at offsets 0 and 1. */
static int annexb_start_code_len(const uint8_t* p, size_t remaining) {
    if (remaining >= 4 &&
        p[0] == 0x00 && p[1] == 0x00 && p[2] == 0x00 && p[3] == 0x01) {
        return 4;
    }
    if (remaining >= 3 &&
        p[0] == 0x00 && p[1] == 0x00 && p[2] == 0x01) {
        return 3;
    }
    return 0;
}

static void emit_span(vsa_nal_span* out, size_t max_out,
                      size_t idx, size_t offset, size_t size) {
    if (out != NULL && idx < max_out) {
        out[idx].offset = offset;
        out[idx].size   = size;
    }
}

size_t vsa_annexb_find_nals(const uint8_t* buf,
                            size_t         size,
                            vsa_nal_span*  out,
                            size_t         max_out) {
    if (buf == NULL || size < 3) {
        return 0;
    }

    size_t count          = 0;
    size_t cur_nal_start  = 0;
    int    have_open_nal  = 0;
    size_t i              = 0;

    while (i < size) {
        const int sc_len = annexb_start_code_len(buf + i, size - i);
        if (sc_len != 0) {
            if (have_open_nal) {
                /* The previous NAL ends immediately before this start
                 * code. Its payload runs from `cur_nal_start` up to
                 * `i` (exclusive). */
                emit_span(out, max_out, count, cur_nal_start, i - cur_nal_start);
                count++;
            }
            cur_nal_start = i + (size_t)sc_len;
            have_open_nal = 1;
            i += (size_t)sc_len;
        } else {
            i++;
        }
    }

    /* Flush the trailing NAL that runs up to end-of-buffer. */
    if (have_open_nal) {
        const size_t tail = cur_nal_start <= size ? size - cur_nal_start : 0;
        emit_span(out, max_out, count, cur_nal_start, tail);
        count++;
    }

    return count;
}
