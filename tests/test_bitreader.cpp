// Unit tests for the pure-C vsa_bit_reader.
//
// Values are pulled directly from ISO/IEC 14496-10 (H.264) §9.1 Table 9-2
// for Exp-Golomb codewords. The multi-byte buffers are constructed by
// manually packing each codeword MSB-first; the test comment blocks show
// the bit-level layout so off-by-one regressions are easy to audit.
#include <doctest/doctest.h>

extern "C" {
#include "codec/bit_reader.h"
}

#include <cstdint>

TEST_CASE("vsa_br_read_bits: reads within a single byte") {
    // 0xD5 = 11010101
    const std::uint8_t data[] = {0xD5};
    vsa_bit_reader br;
    vsa_br_init(&br, data, sizeof(data));

    CHECK(vsa_br_read_bits(&br, 3) == 6u);  // 110
    CHECK(vsa_br_read_bits(&br, 2) == 2u);  //    10
    CHECK(vsa_br_read_bits(&br, 3) == 5u);  //      101
    CHECK(vsa_br_error(&br) == 0);
    CHECK(vsa_br_bits_remaining(&br) == 0u);
}

TEST_CASE("vsa_br_read_bits: crosses byte boundaries cleanly") {
    // 0xD5 0x5A = 11010101 01011010
    const std::uint8_t data[] = {0xD5, 0x5A};
    vsa_bit_reader br;
    vsa_br_init(&br, data, sizeof(data));

    CHECK(vsa_br_read_bits(&br, 5) == 26u);  // 11010
    CHECK(vsa_br_read_bits(&br, 5) == 21u);  //      10101
    CHECK(vsa_br_read_bits(&br, 6) == 26u);  //           011010
    CHECK(vsa_br_error(&br) == 0);
}

TEST_CASE("vsa_br_read_bits: 32-bit wide read") {
    const std::uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    vsa_bit_reader br;
    vsa_br_init(&br, data, sizeof(data));

    CHECK(vsa_br_read_bits(&br, 32) == 0xDEADBEEFu);
    CHECK(vsa_br_error(&br) == 0);
    CHECK(vsa_br_bits_remaining(&br) == 0u);
}

TEST_CASE("vsa_br_read_ue: codeNums 0..8 (H.264 Table 9-2)") {
    // codeNum 0 -> "1"          (1 bit)
    // codeNum 1 -> "010"        (3 bits)
    // codeNum 2 -> "011"        (3 bits)
    // codeNum 3 -> "00100"      (5 bits)
    // codeNum 4 -> "00101"      (5 bits)
    // codeNum 5 -> "00110"      (5 bits)
    // codeNum 6 -> "00111"      (5 bits)
    // codeNum 7 -> "0001000"    (7 bits)
    // codeNum 8 -> "0001001"    (7 bits)
    //
    // Concatenated (41 bits), padded to 48 bits with trailing zeros:
    //   10100110 01000010 10011000 11100010 00000100 10000000
    //   0xA6     0x42     0x98     0xE2     0x04     0x80
    const std::uint8_t data[] = {0xA6, 0x42, 0x98, 0xE2, 0x04, 0x80};
    vsa_bit_reader br;
    vsa_br_init(&br, data, sizeof(data));

    for (std::uint32_t expected = 0; expected <= 8; ++expected) {
        CAPTURE(expected);
        CHECK(vsa_br_read_ue(&br) == expected);
    }
    CHECK(vsa_br_error(&br) == 0);
}

TEST_CASE("vsa_br_read_se: codeNums 0..8 (H.264 §9.1.1 signed mapping)") {
    // Same bit stream as the ue test; expected signed values are:
    //   cN=0 -> 0, cN=1 -> 1, cN=2 -> -1, cN=3 -> 2, cN=4 -> -2,
    //   cN=5 -> 3, cN=6 -> -3, cN=7 -> 4, cN=8 -> -4
    const std::uint8_t data[] = {0xA6, 0x42, 0x98, 0xE2, 0x04, 0x80};
    const std::int32_t expected[] = {0, 1, -1, 2, -2, 3, -3, 4, -4};
    vsa_bit_reader br;
    vsa_br_init(&br, data, sizeof(data));

    for (int i = 0; i < 9; ++i) {
        CAPTURE(i);
        CHECK(vsa_br_read_se(&br) == expected[i]);
    }
    CHECK(vsa_br_error(&br) == 0);
}

TEST_CASE("vsa_br_read_bits: reading past EOF sets the error flag") {
    const std::uint8_t data[] = {0xFF};
    vsa_bit_reader br;
    vsa_br_init(&br, data, sizeof(data));

    CHECK(vsa_br_read_bits(&br, 8) == 0xFFu);
    CHECK(vsa_br_error(&br) == 0);

    (void)vsa_br_read_bits(&br, 1);
    CHECK(vsa_br_error(&br) == 1);
}

TEST_CASE("vsa_br_read_ue: truncated stream sets the error flag") {
    // One byte of all zeros means the leading-zero scan in ue(v) never
    // finds its terminating 1 bit and runs off the end of the buffer.
    const std::uint8_t data[] = {0x00};
    vsa_bit_reader br;
    vsa_br_init(&br, data, sizeof(data));

    (void)vsa_br_read_ue(&br);
    CHECK(vsa_br_error(&br) == 1);
}

TEST_CASE("vsa_br_byte_align: advances to next byte, no-op when aligned") {
    const std::uint8_t data[] = {0xAB, 0xCD};
    vsa_bit_reader br;
    vsa_br_init(&br, data, sizeof(data));

    (void)vsa_br_read_bits(&br, 3);
    vsa_br_byte_align(&br);
    CHECK(vsa_br_bits_consumed(&br) == 8u);
    CHECK(vsa_br_read_bits(&br, 8) == 0xCDu);

    vsa_br_byte_align(&br);  // already aligned: no-op
    CHECK(vsa_br_bits_consumed(&br) == 16u);
}
