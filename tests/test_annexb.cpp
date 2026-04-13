// Unit tests for vsa_annexb_find_nals.
//
// Synthetic streams exercise the corner cases of the byte scanner
// (empty input, leading filler, 3- vs 4-byte start codes, empty NALs
// between adjacent start codes, count-only mode, truncated output).
// A single end-to-end case loads tests/fixtures/tiny_clip.h264 and
// asserts the 13-NAL layout recorded by ffprobe plus a Python
// ground-truth scan.
#include <doctest/doctest.h>

extern "C" {
#include "codec/annexb.h"
}

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

TEST_CASE("vsa_annexb_find_nals: empty inputs") {
    CHECK(vsa_annexb_find_nals(nullptr, 0, nullptr, 0) == 0u);
    const std::uint8_t empty[1] = {0};
    CHECK(vsa_annexb_find_nals(empty, 0, nullptr, 0) == 0u);
}

TEST_CASE("vsa_annexb_find_nals: buffer with no start code") {
    const std::uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    CHECK(vsa_annexb_find_nals(data, sizeof(data), nullptr, 0) == 0u);
}

TEST_CASE("vsa_annexb_find_nals: single 3-byte start code") {
    // 00 00 01 AA  →  one NAL { offset=3, size=1 }
    const std::uint8_t data[] = {0x00, 0x00, 0x01, 0xAA};
    std::array<vsa_nal_span, 4> spans{};
    CHECK(vsa_annexb_find_nals(data, sizeof(data), spans.data(), spans.size()) == 1u);
    CHECK(spans[0].offset == 3u);
    CHECK(spans[0].size   == 1u);
}

TEST_CASE("vsa_annexb_find_nals: single 4-byte start code") {
    // 00 00 00 01 BB  →  one NAL { offset=4, size=1 }
    const std::uint8_t data[] = {0x00, 0x00, 0x00, 0x01, 0xBB};
    std::array<vsa_nal_span, 4> spans{};
    CHECK(vsa_annexb_find_nals(data, sizeof(data), spans.data(), spans.size()) == 1u);
    CHECK(spans[0].offset == 4u);
    CHECK(spans[0].size   == 1u);
}

TEST_CASE("vsa_annexb_find_nals: two NALs with mixed 3/4-byte start codes") {
    const std::uint8_t data[] = {
        0x00, 0x00, 0x00, 0x01,
        0xAA, 0xBB,
        0x00, 0x00, 0x01,
        0xCC,
    };
    std::array<vsa_nal_span, 4> spans{};
    CHECK(vsa_annexb_find_nals(data, sizeof(data), spans.data(), spans.size()) == 2u);
    CHECK(spans[0].offset == 4u);
    CHECK(spans[0].size   == 2u);
    CHECK(spans[1].offset == 9u);
    CHECK(spans[1].size   == 1u);
}

TEST_CASE("vsa_annexb_find_nals: leading bytes before first start code are ignored") {
    const std::uint8_t data[] = {0x77, 0x88, 0x99, 0x00, 0x00, 0x01, 0xAA};
    std::array<vsa_nal_span, 2> spans{};
    CHECK(vsa_annexb_find_nals(data, sizeof(data), spans.data(), spans.size()) == 1u);
    CHECK(spans[0].offset == 6u);
    CHECK(spans[0].size   == 1u);
}

TEST_CASE("vsa_annexb_find_nals: count-only mode (out=nullptr, max_out=0)") {
    const std::uint8_t data[] = {
        0x00, 0x00, 0x01, 0xAA,
        0x00, 0x00, 0x01, 0xBB, 0xCC,
        0x00, 0x00, 0x00, 0x01, 0xDD,
    };
    CHECK(vsa_annexb_find_nals(data, sizeof(data), nullptr, 0) == 3u);
}

TEST_CASE("vsa_annexb_find_nals: output array smaller than NAL count") {
    // Three NALs but only room for one span: the total count is still
    // returned so the caller can tell it was truncated.
    const std::uint8_t data[] = {
        0x00, 0x00, 0x01, 0xAA,
        0x00, 0x00, 0x01, 0xBB,
        0x00, 0x00, 0x01, 0xCC,
    };
    std::array<vsa_nal_span, 1> spans{};
    CHECK(vsa_annexb_find_nals(data, sizeof(data), spans.data(), spans.size()) == 3u);
    CHECK(spans[0].offset == 3u);
    CHECK(spans[0].size   == 1u);
}

TEST_CASE("vsa_annexb_find_nals: adjacent start codes produce an empty NAL") {
    // 00 00 01 00 00 01 AA  →  first NAL is empty, second contains AA.
    const std::uint8_t data[] = {0x00, 0x00, 0x01, 0x00, 0x00, 0x01, 0xAA};
    std::array<vsa_nal_span, 2> spans{};
    CHECK(vsa_annexb_find_nals(data, sizeof(data), spans.data(), spans.size()) == 2u);
    CHECK(spans[0].size   == 0u);
    CHECK(spans[1].offset == 6u);
    CHECK(spans[1].size   == 1u);
}

// CMake passes VSA_FIXTURES_DIR via target_compile_definitions so this
// test works from both the build tree (ctest) and a plain compile.
#ifndef VSA_FIXTURES_DIR
#define VSA_FIXTURES_DIR "tests/fixtures"
#endif

TEST_CASE("vsa_annexb_find_nals: tiny_clip.h264 has the expected 13-NAL layout") {
    const std::string path = std::string(VSA_FIXTURES_DIR) + "/tiny_clip.h264";
    std::FILE* f = std::fopen(path.c_str(), "rb");
    REQUIRE_MESSAGE(f != nullptr, "failed to open " << path);
    std::fseek(f, 0, SEEK_END);
    const long fs = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    std::vector<std::uint8_t> buf(static_cast<std::size_t>(fs));
    CHECK(std::fread(buf.data(), 1, buf.size(), f) == buf.size());
    std::fclose(f);

    std::array<vsa_nal_span, 32> spans{};
    const std::size_t n = vsa_annexb_find_nals(
        buf.data(), buf.size(), spans.data(), spans.size());
    CHECK(n == 13u);
    CHECK(spans[0].offset == 4u);

    // NAL unit types derived from ffprobe + a Python start-code scan:
    //   SPS, PPS, SEI, IDR, P, P, P, SPS, PPS, IDR, P, P, P
    const std::uint8_t expected[13] = {7, 8, 6, 5, 1, 1, 1, 7, 8, 5, 1, 1, 1};
    for (std::size_t k = 0; k < n && k < 13; ++k) {
        CAPTURE(k);
        const std::uint8_t hdr  = buf[spans[k].offset];
        const std::uint8_t type = static_cast<std::uint8_t>(hdr & 0x1Fu);
        CHECK(type == expected[k]);
    }
}
