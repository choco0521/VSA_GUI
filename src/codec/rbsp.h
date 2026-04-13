#ifndef VSA_CODEC_RBSP_H
#define VSA_CODEC_RBSP_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Remove H.264 emulation_prevention_three_byte (0x03) escapes from a
 * NAL unit RBSP, as defined in ISO/IEC 14496-10 §7.4.1.1.
 *
 * Whenever the byte sequence  00 00 03  appears in the input, the 0x03
 * byte is dropped so the output contains  00 00  instead. All other
 * bytes are copied verbatim. The transform is purely a read-side
 * inverse of the encoder's escape logic and never inspects the byte
 * that follows the 0x03 — streams that use the escape as a trailing
 * stop-bit padding are handled identically.
 *
 * The caller must provide an output buffer of at least `in_size`
 * bytes; `out` and `in` must not overlap. Returns the number of bytes
 * written to `out`.
 *
 * If `in` is NULL or `in_size` is 0 the function returns 0 without
 * touching `out`.
 */
size_t vsa_rbsp_unescape(const uint8_t* in,
                         size_t         in_size,
                         uint8_t*       out);

#ifdef __cplusplus
}
#endif

#endif /* VSA_CODEC_RBSP_H */
