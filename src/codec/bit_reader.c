#include "bit_reader.h"

void vsa_br_init(vsa_bit_reader* br, const uint8_t* data, size_t size) {
    br->data     = data;
    br->size     = size;
    br->byte_pos = 0;
    br->bit_pos  = 0;
    br->error    = 0;
}

int vsa_br_error(const vsa_bit_reader* br) {
    return br->error;
}

size_t vsa_br_bits_consumed(const vsa_bit_reader* br) {
    return br->byte_pos * (size_t)8 + (size_t)br->bit_pos;
}

size_t vsa_br_bits_remaining(const vsa_bit_reader* br) {
    const size_t total = br->size * (size_t)8;
    const size_t used  = vsa_br_bits_consumed(br);
    return used >= total ? (size_t)0 : total - used;
}

uint32_t vsa_br_read_bits(vsa_bit_reader* br, int n) {
    if (n <= 0 || n > 32) {
        br->error = 1;
        return 0;
    }
    if (vsa_br_bits_remaining(br) < (size_t)n) {
        /* Snap to EOF so subsequent reads also fail fast. */
        br->byte_pos = br->size;
        br->bit_pos  = 0;
        br->error    = 1;
        return 0;
    }

    uint32_t value = 0;
    while (n > 0) {
        const int bits_left_in_byte = 8 - br->bit_pos;
        const int take = n < bits_left_in_byte ? n : bits_left_in_byte;
        const uint8_t byte = br->data[br->byte_pos];

        /* Take the `take` most-significant remaining bits of `byte`. */
        const int shift = bits_left_in_byte - take;
        const uint32_t mask  = ((uint32_t)1 << take) - 1u;
        const uint32_t chunk = ((uint32_t)byte >> shift) & mask;

        value = (value << take) | chunk;

        br->bit_pos += take;
        if (br->bit_pos == 8) {
            br->bit_pos = 0;
            br->byte_pos += 1;
        }
        n -= take;
    }
    return value;
}

uint32_t vsa_br_read_u1(vsa_bit_reader* br) {
    return vsa_br_read_bits(br, 1);
}

uint32_t vsa_br_read_ue(vsa_bit_reader* br) {
    /* Count leading zero bits, then read that many suffix bits.
     * codeNum = (1 << zeros) - 1 + suffix. H.264 §9.1. */
    int zeros = 0;
    for (;;) {
        if (br->error || vsa_br_bits_remaining(br) == 0) {
            br->error = 1;
            return 0;
        }
        const uint32_t bit = vsa_br_read_bits(br, 1);
        if (br->error) {
            return 0;
        }
        if (bit == 1u) {
            break;
        }
        zeros += 1;
        if (zeros > 32) {
            /* Malformed stream: ue(v) supports at most 32 leading zeros. */
            br->error = 1;
            return 0;
        }
    }

    if (zeros == 0) {
        return 0;
    }
    const uint32_t suffix = vsa_br_read_bits(br, zeros);
    if (br->error) {
        return 0;
    }
    return (((uint32_t)1 << zeros) - 1u) + suffix;
}

int32_t vsa_br_read_se(vsa_bit_reader* br) {
    const uint32_t k = vsa_br_read_ue(br);
    if (br->error) {
        return 0;
    }
    /* H.264 §9.1.1: codeNum → signed mapping.
     *   k odd  →  (k + 1) / 2
     *   k even →  -(k / 2)
     * Works for codeNum 0 (→0) through 2^31-1. */
    if ((k & 1u) != 0u) {
        return (int32_t)((k + 1u) >> 1);
    }
    return -(int32_t)(k >> 1);
}

void vsa_br_byte_align(vsa_bit_reader* br) {
    if (br->bit_pos != 0) {
        br->bit_pos  = 0;
        br->byte_pos += 1;
    }
}
