// Unit tests for vsa_h264_parse_sps.
//
// The reference values come from ffprobe's output on
// tests/fixtures/tiny_clip.h264 (a libx264 Constrained Baseline
// 64x48 clip):
//
//     profile            : Constrained Baseline (66, constraint_set1=1)
//     level              : 10 (1.0)
//     width x height     : 64 x 48 yuv420p
//     pic_order_cnt_type : 2 (x264 baseline default)
//
// The RBSP byte string below was extracted from the file by the
// Phase C scanner, had its NAL header byte stripped, and was run
// through vsa_rbsp_unescape() so one 00 00 03 → 00 00 escape is
// already removed. Keeping the expected buffer inline makes this
// test hermetic — it runs even if the fixture file is missing.

#include <doctest/doctest.h>

extern "C" {
#include "codec/h264_sps.h"
}

#include <cstdint>

namespace {

constexpr std::uint8_t kTinyClipSpsRbsp[] = {
    0x42, 0xC0, 0x0A, 0xDA, 0x11, 0xEC, 0x04, 0x40,
    0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x04, 0x03,
    0xC4, 0x89, 0xA8,
};

}  // namespace

TEST_CASE("vsa_h264_parse_sps: tiny_clip.h264 Constrained Baseline 64x48") {
    vsa_h264_sps sps{};
    const int rc = vsa_h264_parse_sps(
        kTinyClipSpsRbsp, sizeof(kTinyClipSpsRbsp), &sps);
    REQUIRE(rc == 0);

    CHECK(sps.profile_idc         == 66u);
    CHECK(sps.constraint_set0_flag == 1u);
    CHECK(sps.constraint_set1_flag == 1u);  // → "Constrained"
    CHECK(sps.constraint_set2_flag == 0u);
    CHECK(sps.constraint_set3_flag == 0u);
    CHECK(sps.constraint_set4_flag == 0u);
    CHECK(sps.constraint_set5_flag == 0u);
    CHECK(sps.level_idc           == 10u);
    CHECK(sps.seq_parameter_set_id == 0u);

    // Baseline defaults for fields absent outside the high profiles.
    CHECK(sps.chroma_format_idc       == 1u);  // 4:2:0
    CHECK(sps.bit_depth_luma_minus8   == 0u);
    CHECK(sps.bit_depth_chroma_minus8 == 0u);

    CHECK(sps.log2_max_frame_num_minus4 == 0u);
    CHECK(sps.pic_order_cnt_type        == 2u);
    CHECK(sps.max_num_ref_frames        == 1u);

    // 4 x 3 macroblocks → 64 x 48 luma pixels.
    CHECK(sps.pic_width_in_mbs_minus1        == 3u);
    CHECK(sps.pic_height_in_map_units_minus1 == 2u);
    CHECK(sps.frame_mbs_only_flag            == 1u);
    CHECK(sps.direct_8x8_inference_flag      == 1u);
    CHECK(sps.frame_cropping_flag            == 0u);

    // x264 emits a VUI block even for baseline, mostly for
    // aspect_ratio_info and timing.
    CHECK(sps.vui_parameters_present_flag == 1u);
}

TEST_CASE("vsa_h264_sps_picture_size: derives 64 x 48 from tiny_clip SPS") {
    vsa_h264_sps sps{};
    REQUIRE(vsa_h264_parse_sps(
        kTinyClipSpsRbsp, sizeof(kTinyClipSpsRbsp), &sps) == 0);

    std::uint32_t w = 0;
    std::uint32_t h = 0;
    REQUIRE(vsa_h264_sps_picture_size(&sps, &w, &h) == 0);
    CHECK(w == 64u);
    CHECK(h == 48u);
}

TEST_CASE("vsa_h264_parse_sps: null / empty inputs are rejected") {
    vsa_h264_sps sps{};
    CHECK(vsa_h264_parse_sps(nullptr, 0, &sps) < 0);
    CHECK(vsa_h264_parse_sps(kTinyClipSpsRbsp, 0, &sps) < 0);

    const std::uint8_t one_byte[] = {0x42};
    CHECK(vsa_h264_parse_sps(one_byte, sizeof(one_byte), &sps) < 0);
}

TEST_CASE("vsa_h264_parse_sps: out parameter must not be null") {
    CHECK(vsa_h264_parse_sps(
        kTinyClipSpsRbsp, sizeof(kTinyClipSpsRbsp), nullptr) < 0);
}

TEST_CASE("vsa_h264_sps_picture_size: cropping trims both dimensions") {
    // Hand-built SPS struct: 128x80 coded, crop 8 px off each side.
    vsa_h264_sps sps{};
    sps.chroma_format_idc              = 1;
    sps.pic_width_in_mbs_minus1        = 7;   // 8 * 16 = 128
    sps.pic_height_in_map_units_minus1 = 4;   // 5 * 16 = 80
    sps.frame_mbs_only_flag            = 1;
    sps.frame_cropping_flag            = 1;
    sps.frame_crop_left_offset         = 4;   // * SubWidthC  (2) = 8
    sps.frame_crop_right_offset        = 4;   // * SubWidthC  (2) = 8
    sps.frame_crop_top_offset          = 4;   // * SubHeightC (2) = 8
    sps.frame_crop_bottom_offset       = 4;   // * SubHeightC (2) = 8

    std::uint32_t w = 0;
    std::uint32_t h = 0;
    REQUIRE(vsa_h264_sps_picture_size(&sps, &w, &h) == 0);
    CHECK(w == 128u - 16u);  // 8 left + 8 right = 16
    CHECK(h == 80u  - 16u);  // 8 top  + 8 bottom = 16
}
