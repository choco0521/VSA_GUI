// Integration test for vsa_annexb_find_nals against a real H.264
// elementary stream (Elecard's TheaterSquare 1080p sample).
//
// The file is never committed to the repository — GitHub Actions
// downloads it via tests/scripts/fetch_elecard_sample.sh, converts the
// MP4 container to raw Annex B, and exports the path through the
// TEST_ELECARD_STREAM environment variable. When the variable is
// absent (local builds, or a CI run where the fetch step was skipped
// because elecard.com was unreachable) the test early-returns with a
// message, so the suite remains green.
//
// The assertions are deliberately coarse. The goal at Phase C-extra
// is to prove the start-code scanner survives a production-grade
// stream; the tight, bit-level checks against JM live in Phase D.

#include <doctest/doctest.h>

extern "C" {
#include "codec/annexb.h"
}

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

TEST_CASE("vsa_annexb_find_nals: Elecard TheaterSquare 1080p stream") {
    const char* path = std::getenv("TEST_ELECARD_STREAM");
    if (path == nullptr || *path == '\0') {
        MESSAGE("TEST_ELECARD_STREAM not set — skipping Elecard integration test");
        return;
    }

    std::FILE* f = std::fopen(path, "rb");
    if (f == nullptr) {
        MESSAGE("failed to open Elecard stream at " << path << " — skipping");
        return;
    }

    std::fseek(f, 0, SEEK_END);
    const long fs = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    REQUIRE(fs > 0);

    std::vector<std::uint8_t> buf(static_cast<std::size_t>(fs));
    REQUIRE(std::fread(buf.data(), 1, buf.size(), f) == buf.size());
    std::fclose(f);

    // First pass: count-only mode.
    const std::size_t nal_count =
        vsa_annexb_find_nals(buf.data(), buf.size(), nullptr, 0);
    MESSAGE("TheaterSquare NAL count: " << nal_count
            << " (file size " << buf.size() << " bytes)");
    CHECK(nal_count > 100u);  // A 1080p clip at normal bitrates has many slices.

    // Second pass: record every span so we can inspect NAL header types.
    std::vector<vsa_nal_span> spans(nal_count);
    const std::size_t n2 = vsa_annexb_find_nals(
        buf.data(), buf.size(), spans.data(), spans.size());
    CHECK(n2 == nal_count);

    std::size_t sps_count     = 0;
    std::size_t pps_count     = 0;
    std::size_t idr_count     = 0;
    std::size_t non_idr_count = 0;

    for (const vsa_nal_span& s : spans) {
        if (s.size == 0) {
            continue;
        }
        const std::uint8_t nal_type =
            static_cast<std::uint8_t>(buf[s.offset] & 0x1Fu);
        switch (nal_type) {
            case 1:  ++non_idr_count; break;  // coded slice of a non-IDR picture
            case 5:  ++idr_count;     break;  // coded slice of an IDR picture
            case 7:  ++sps_count;     break;  // SPS
            case 8:  ++pps_count;     break;  // PPS
            default: break;
        }
    }

    MESSAGE("TheaterSquare: SPS=" << sps_count
            << " PPS=" << pps_count
            << " IDR=" << idr_count
            << " non-IDR=" << non_idr_count);

    // Sanity: a real, decodable H.264 stream must carry at least one
    // SPS, one PPS, one IDR and one further coded slice.
    CHECK(sps_count     >= 1u);
    CHECK(pps_count     >= 1u);
    CHECK(idr_count     >= 1u);
    CHECK(non_idr_count >= 1u);
}
