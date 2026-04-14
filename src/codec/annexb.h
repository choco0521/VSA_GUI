#ifndef VSA_CODEC_ANNEXB_H
#define VSA_CODEC_ANNEXB_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * A NAL unit span within an H.264 Annex B byte stream.
 *
 *   offset  — byte index of the first byte of the NAL unit payload
 *             (the NAL header byte), i.e. the byte *after* the start
 *             code.
 *   size    — number of payload bytes, not including any trailing
 *             start code that begins the next NAL unit.
 *
 * A span with size == 0 represents two adjacent start codes and is
 * reported as-is so callers can detect malformed streams.
 */
typedef struct vsa_nal_span {
    size_t offset;
    size_t size;
} vsa_nal_span;

/*
 * Scan an Annex B byte buffer for NAL unit start codes (ISO/IEC
 * 14496-10 §B.1) and record every resulting span.
 *
 * Matched start codes:
 *     3-byte: 00 00 01
 *     4-byte: 00 00 00 01
 *
 * The 4-byte form is tested before the 3-byte form so a 4-byte start
 * code at offset `i` does not get shadowed by a 3-byte prefix ending
 * at i+2.
 *
 * Bytes before the first start code are treated as leading filler and
 * ignored. After the final start code, the tail of the buffer is
 * reported as the last NAL unit.
 *
 * If `out` is non-NULL, the first min(nal_count, max_out) spans are
 * written to it. The total number of NAL units found is always
 * returned, so callers can detect truncation by comparing against
 * max_out.
 *
 * Passing out=NULL, max_out=0 turns the call into a pure count.
 */
size_t vsa_annexb_find_nals(const uint8_t* buf,
                            size_t         size,
                            vsa_nal_span*  out,
                            size_t         max_out);

#ifdef __cplusplus
}
#endif

#endif /* VSA_CODEC_ANNEXB_H */
