#include "rbsp.h"

size_t vsa_rbsp_unescape(const uint8_t* in, size_t in_size, uint8_t* out) {
    if (in == NULL || in_size == 0) {
        return 0;
    }

    size_t oi = 0;
    size_t i  = 0;
    while (i < in_size) {
        /* Detect the pattern  00 00 03  and emit only the two leading
         * zeros, skipping the 0x03 escape byte. */
        if (i + 2 < in_size &&
            in[i]     == 0x00 &&
            in[i + 1] == 0x00 &&
            in[i + 2] == 0x03) {
            out[oi++] = 0x00;
            out[oi++] = 0x00;
            i += 3;
        } else {
            out[oi++] = in[i++];
        }
    }
    return oi;
}
