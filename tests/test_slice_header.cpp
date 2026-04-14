// End-to-end H.264 slice header tests using the existing
// tests/fixtures/tiny_clip.h264 fixture. The test loads the file,
// runs it through the Phase C Annex B scanner, finds the first SPS
// and parses it with the Phase D.1 SPS parser, then locates the
// first IDR (NAL type 5) and the first non-IDR (NAL type 1) slice
// and parses each header with vsa_h264_parse_slice_header. The
// result is cross-checked against ffprobe ground truth: tiny_clip
// is "I, P, P, P, I, P, P, P" with all-I and all-P slice types.

#include <doctest/doctest.h>

extern "C" {
#include "codec/annexb.h"
#include "codec/h264_slice_header.h"
#include "codec/h264_sps.h"
#include "codec/rbsp.h"
}

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#ifndef VSA_FIXTURES_DIR
#define VSA_FIXTURES_DIR "tests/fixtures"
#endif

namespace {

std::vector<std::uint8_t> read_tiny_clip() {
    const std::string path = std::string(VSA_FIXTURES_DIR) + "/tiny_clip.h264";
    std::FILE* f = std::fopen(path.c_str(), "rb");
    REQUIRE(f != nullptr);
    std::fseek(f, 0, SEEK_END);
    const long sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    std::vector<std::uint8_t> buf(static_cast<std::size_t>(sz));
    REQUIRE(std::fread(buf.data(), 1, buf.size(), f) == buf.size());
    std::fclose(f);
    return buf;
}

// Strip the NAL header byte and run vsa_rbsp_unescape on the rest.
std::vector<std::uint8_t> nal_payload_to_rbsp(
    const std::uint8_t* buf, vsa_nal_span span) {
    REQUIRE(span.size >= 1u);
    const std::uint8_t* in = buf + span.offset + 1;
    const std::size_t   in_size = span.size - 1u;
    std::vector<std::uint8_t> out(in_size);
    const std::size_t out_size = vsa_rbsp_unescape(in, in_size, out.data());
    out.resize(out_size);
    return out;
}

// Find the first NAL whose header byte encodes the requested type.
bool find_first_nal_of_type(
    const std::uint8_t*       buf,
    const vsa_nal_span*       spans,
    std::size_t               span_count,
    std::uint8_t              wanted_type,
    vsa_nal_span*             out_span) {
    for (std::size_t i = 0; i < span_count; ++i) {
        if (spans[i].size == 0) continue;
        const std::uint8_t hdr = buf[spans[i].offset];
        const std::uint8_t typ = static_cast<std::uint8_t>(hdr & 0x1Fu);
        if (typ == wanted_type) {
            *out_span = spans[i];
            return true;
        }
    }
    return false;
}

vsa_h264_sps parse_first_sps(
    const std::uint8_t* buf, std::size_t buf_size) {
    std::array<vsa_nal_span, 32> spans{};
    const std::size_t n = vsa_annexb_find_nals(
        buf, buf_size, spans.data(), spans.size());
    REQUIRE(n >= 1u);

    vsa_nal_span sps_span{};
    REQUIRE(find_first_nal_of_type(buf, spans.data(), n, 7u, &sps_span));

    auto rbsp = nal_payload_to_rbsp(buf, sps_span);
    vsa_h264_sps sps{};
    REQUIRE(vsa_h264_parse_sps(rbsp.data(), rbsp.size(), &sps) == 0);
    return sps;
}

}  // namespace

TEST_CASE("vsa_h264_parse_slice_header: tiny_clip first IDR is an I-slice") {
    const auto data = read_tiny_clip();
    const auto sps  = parse_first_sps(data.data(), data.size());
    REQUIRE(sps.profile_idc == 66u);
    REQUIRE(sps.pic_order_cnt_type == 2u);  // x264 baseline default

    std::array<vsa_nal_span, 32> spans{};
    const std::size_t n = vsa_annexb_find_nals(
        data.data(), data.size(), spans.data(), spans.size());

    vsa_nal_span idr_span{};
    REQUIRE(find_first_nal_of_type(data.data(), spans.data(), n, 5u, &idr_span));

    const auto rbsp = nal_payload_to_rbsp(data.data(), idr_span);

    vsa_h264_slice_header sh{};
    const int rc = vsa_h264_parse_slice_header(
        rbsp.data(), rbsp.size(), 5u, &sps, &sh);
    REQUIRE(rc == 0);

    CHECK(sh.first_mb_in_slice    == 0u);
    CHECK((sh.slice_type % 5u)    == 2u);    // I
    CHECK(vsa_h264_slice_type_letter(sh.slice_type) == 'I');
    CHECK(sh.has_idr_pic_id       == 1);
    CHECK(sh.has_pic_order_cnt_lsb == 0);     // poc_type == 2
    CHECK(sh.field_pic_flag       == 0);      // frame_mbs_only
}

TEST_CASE("vsa_h264_parse_slice_header: tiny_clip first non-IDR is a P-slice") {
    const auto data = read_tiny_clip();
    const auto sps  = parse_first_sps(data.data(), data.size());

    std::array<vsa_nal_span, 32> spans{};
    const std::size_t n = vsa_annexb_find_nals(
        data.data(), data.size(), spans.data(), spans.size());

    vsa_nal_span p_span{};
    REQUIRE(find_first_nal_of_type(data.data(), spans.data(), n, 1u, &p_span));

    const auto rbsp = nal_payload_to_rbsp(data.data(), p_span);

    vsa_h264_slice_header sh{};
    const int rc = vsa_h264_parse_slice_header(
        rbsp.data(), rbsp.size(), 1u, &sps, &sh);
    REQUIRE(rc == 0);

    CHECK(sh.first_mb_in_slice    == 0u);
    CHECK((sh.slice_type % 5u)    == 0u);    // P
    CHECK(vsa_h264_slice_type_letter(sh.slice_type) == 'P');
    CHECK(sh.has_idr_pic_id       == 0);     // non-IDR
    CHECK(sh.has_pic_order_cnt_lsb == 0);    // poc_type == 2
}

TEST_CASE("vsa_h264_slice_type_letter: maps known slice_type values") {
    // Per H.264 Table 7-6:
    //   0 P, 1 B, 2 I, 3 SP, 4 SI, 5..9 are the "all-of" variants
    //   that share their base letter.
    CHECK(vsa_h264_slice_type_letter(0) == 'P');
    CHECK(vsa_h264_slice_type_letter(1) == 'B');
    CHECK(vsa_h264_slice_type_letter(2) == 'I');
    CHECK(vsa_h264_slice_type_letter(3) == 'S');
    CHECK(vsa_h264_slice_type_letter(4) == 'i');
    CHECK(vsa_h264_slice_type_letter(5) == 'P');
    CHECK(vsa_h264_slice_type_letter(6) == 'B');
    CHECK(vsa_h264_slice_type_letter(7) == 'I');
    CHECK(vsa_h264_slice_type_letter(8) == 'S');
    CHECK(vsa_h264_slice_type_letter(9) == 'i');
}

TEST_CASE("vsa_h264_parse_slice_header: rejects null inputs") {
    vsa_h264_sps sps{};
    vsa_h264_slice_header sh{};
    const std::uint8_t one[] = {0xAA};
    CHECK(vsa_h264_parse_slice_header(nullptr, 0, 1, &sps, &sh) < 0);
    CHECK(vsa_h264_parse_slice_header(one, sizeof(one), 1, nullptr, &sh) < 0);
    CHECK(vsa_h264_parse_slice_header(one, sizeof(one), 1, &sps, nullptr) < 0);
    CHECK(vsa_h264_parse_slice_header(one, 0, 1, &sps, &sh) < 0);
}
