// Unit tests for the H.264 RBSP emulation-prevention escape removal
// defined in ISO/IEC 14496-10 §7.4.1.1.
#include <doctest/doctest.h>

extern "C" {
#include "codec/rbsp.h"
}

#include <array>
#include <cstdint>
#include <cstring>

TEST_CASE("vsa_rbsp_unescape: pass-through when no escapes are present") {
    const std::uint8_t in[]       = {0x01, 0x02, 0x03, 0x04};
    std::array<std::uint8_t, 4> out{};
    const std::size_t n = vsa_rbsp_unescape(in, sizeof(in), out.data());
    CHECK(n == 4u);
    CHECK(std::memcmp(in, out.data(), n) == 0);
}

TEST_CASE("vsa_rbsp_unescape: single 00 00 03 sequence") {
    // 00 00 03 00  →  00 00 00
    const std::uint8_t in[] = {0x00, 0x00, 0x03, 0x00};
    std::array<std::uint8_t, 4> out{};
    const std::size_t n = vsa_rbsp_unescape(in, sizeof(in), out.data());
    CHECK(n == 3u);
    CHECK(out[0] == 0x00);
    CHECK(out[1] == 0x00);
    CHECK(out[2] == 0x00);
}

TEST_CASE("vsa_rbsp_unescape: multiple escapes interleaved with payload") {
    // 0A 00 00 03 01 00 00 03 FF   →   0A 00 00 01 00 00 FF
    const std::uint8_t in[] = {
        0x0A, 0x00, 0x00, 0x03, 0x01, 0x00, 0x00, 0x03, 0xFF};
    std::array<std::uint8_t, 9> out{};
    const std::size_t n = vsa_rbsp_unescape(in, sizeof(in), out.data());
    const std::uint8_t expected[] = {
        0x0A, 0x00, 0x00, 0x01, 0x00, 0x00, 0xFF};
    REQUIRE(n == sizeof(expected));
    CHECK(std::memcmp(out.data(), expected, n) == 0);
}

TEST_CASE("vsa_rbsp_unescape: 0x03 byte not preceded by two zeros is preserved") {
    // 00 01 03 → should stay as-is (only 00 00 03 is a legal escape)
    const std::uint8_t in[] = {0x00, 0x01, 0x03};
    std::array<std::uint8_t, 3> out{};
    const std::size_t n = vsa_rbsp_unescape(in, sizeof(in), out.data());
    CHECK(n == 3u);
    CHECK(std::memcmp(in, out.data(), n) == 0);
}

TEST_CASE("vsa_rbsp_unescape: null/empty input is a no-op") {
    std::array<std::uint8_t, 4> out{};
    CHECK(vsa_rbsp_unescape(nullptr, 0, out.data()) == 0u);
    const std::uint8_t empty[1] = {0};
    CHECK(vsa_rbsp_unescape(empty, 0, out.data()) == 0u);
}
