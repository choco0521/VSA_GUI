#ifndef VSA_CODEC_BIT_READER_H
#define VSA_CODEC_BIT_READER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * MSB-first bit reader over a byte buffer. Models RBSP consumption in
 * H.264 / H.265 parsers (ISO/IEC 14496-10 §7.2, §9.1).
 *
 * Bit positions count from the MSB of the current byte: bit_pos == 0
 * means the next read will consume bit 7 of data[byte_pos] (the most
 * significant bit of that byte), which is the convention used by the
 * H.264 specification.
 *
 * Reads past the end of the buffer set `error` to 1 and return 0. Once
 * `error` is set it remains sticky until the reader is re-initialized.
 * Callers should either check vsa_br_error() after each top-level parse
 * or rely on downstream sanity checks (e.g. a slice with zero width).
 */
typedef struct vsa_bit_reader {
    const uint8_t* data;
    size_t         size;      /* total buffer length in bytes            */
    size_t         byte_pos;  /* next byte to touch (may equal size)     */
    int            bit_pos;   /* 0..7, 0 = aligned on byte boundary      */
    int            error;     /* sticky EOF / invalid-read flag          */
} vsa_bit_reader;

void vsa_br_init(vsa_bit_reader* br, const uint8_t* data, size_t size);

int  vsa_br_error(const vsa_bit_reader* br);

/* Total bits consumed since vsa_br_init. */
size_t vsa_br_bits_consumed(const vsa_bit_reader* br);

/* Total bits still available. Zero at EOF. */
size_t vsa_br_bits_remaining(const vsa_bit_reader* br);

/* Read `n` bits (1..32) as an unsigned integer, MSB-first. */
uint32_t vsa_br_read_bits(vsa_bit_reader* br, int n);

/* Single-bit convenience wrapper. */
uint32_t vsa_br_read_u1(vsa_bit_reader* br);

/* H.264 unsigned Exp-Golomb code, ISO/IEC 14496-10 §9.1. */
uint32_t vsa_br_read_ue(vsa_bit_reader* br);

/* H.264 signed Exp-Golomb code, ISO/IEC 14496-10 §9.1.1. */
int32_t vsa_br_read_se(vsa_bit_reader* br);

/* Advance to the next byte boundary. No-op when already aligned. */
void vsa_br_byte_align(vsa_bit_reader* br);

#ifdef __cplusplus
}
#endif

#endif /* VSA_CODEC_BIT_READER_H */
