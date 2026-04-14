// vsa_parse — headless CLI that exercises the vsa_codec C library from
// C++. Walks an H.264 Annex B byte stream and emits structured metadata
// in one of three formats, selected by the --dump-sps / --dump-slices /
// --json flag. The output is designed to be diff-friendly: key=value
// lines for dumps, canonical JSON for machine consumption.
//
// This CLI is the bridge between the pure-C codec library and the
// several downstream consumers:
//   - Phase D.3 reference diffs (ffprobe / JM trace comparison)
//   - Phase F Qt GUI, which invokes the same parsing path via an
//     in-process library call
//   - Phase G demuxer facade, which pipes elementary streams from
//     ffmpeg into vsa_parse
//
// Usage:
//     vsa_parse --version
//     vsa_parse <file.h264> --dump-sps
//     vsa_parse <file.h264> --dump-slices
//     vsa_parse <file.h264> --json
//
// Exit codes:
//     0   success
//     2   usage error
//     3   file not found / unreadable
//     4   no SPS NAL found in the stream
//     5   SPS parse error
//     6   slice header parse error

extern "C" {
#include "codec/annexb.h"
#include "codec/codec_version.h"
#include "codec/h264_nalu.h"
#include "codec/h264_slice_header.h"
#include "codec/h264_sps.h"
#include "codec/rbsp.h"
}

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

enum class Mode {
    kNone,
    kVersion,
    kDumpSps,
    kDumpSlices,
    kJson,
};

void print_usage(const char* argv0) {
    std::fprintf(stderr,
        "usage:\n"
        "  %s --version\n"
        "  %s <file.h264> --dump-sps\n"
        "  %s <file.h264> --dump-slices\n"
        "  %s <file.h264> --json\n"
        "\n"
        "Parse a raw H.264 Annex B byte stream and emit structured\n"
        "metadata. Output formats are stable and designed for diffing\n"
        "against reference decoders (ffprobe, JM ldecod).\n",
        argv0, argv0, argv0, argv0);
}

std::vector<std::uint8_t> read_file(const char* path) {
    std::FILE* f = std::fopen(path, "rb");
    if (!f) {
        std::fprintf(stderr, "vsa_parse: cannot open %s\n", path);
        std::exit(3);
    }
    std::fseek(f, 0, SEEK_END);
    const long sz = std::ftell(f);
    if (sz < 0) {
        std::fprintf(stderr, "vsa_parse: ftell failed on %s\n", path);
        std::fclose(f);
        std::exit(3);
    }
    std::fseek(f, 0, SEEK_SET);
    std::vector<std::uint8_t> buf(static_cast<std::size_t>(sz));
    const std::size_t n = std::fread(buf.data(), 1, buf.size(), f);
    std::fclose(f);
    if (n != buf.size()) {
        std::fprintf(stderr, "vsa_parse: short read on %s (%zu/%zu)\n",
                     path, n, buf.size());
        std::exit(3);
    }
    return buf;
}

std::vector<std::uint8_t> nal_rbsp(const std::uint8_t* buf,
                                   const vsa_nal_span& span) {
    // Skip the 1-byte NAL header and run the emulation-prevention
    // reverse transform on the payload.
    if (span.size < 1) return {};
    const std::uint8_t* in = buf + span.offset + 1;
    const std::size_t   in_size = span.size - 1u;
    std::vector<std::uint8_t> out(in_size);
    const std::size_t out_size = vsa_rbsp_unescape(in, in_size, out.data());
    out.resize(out_size);
    return out;
}

// Locate the first SPS NAL in the stream, parse it, and return (sps,
// ok). Writes an error to stderr and returns ok=false on failure.
struct SpsResult {
    vsa_h264_sps sps;
    bool         ok;
};

SpsResult find_and_parse_sps(const std::vector<std::uint8_t>&   data,
                             const std::vector<vsa_nal_span>&   spans) {
    SpsResult result{};
    for (const auto& s : spans) {
        if (s.size == 0) continue;
        const std::uint8_t nal_type =
            static_cast<std::uint8_t>(data[s.offset] & 0x1Fu);
        if (nal_type != VSA_H264_NAL_SPS) continue;
        const auto rbsp = nal_rbsp(data.data(), s);
        const int rc = vsa_h264_parse_sps(rbsp.data(), rbsp.size(), &result.sps);
        if (rc != 0) {
            std::fprintf(stderr, "vsa_parse: SPS parse error rc=%d\n", rc);
            result.ok = false;
            return result;
        }
        result.ok = true;
        return result;
    }
    std::fprintf(stderr, "vsa_parse: no SPS NAL found\n");
    result.ok = false;
    return result;
}

void dump_sps(const vsa_h264_sps& sps) {
    // key=value output, one line per field. Stable names match
    // ISO/IEC 14496-10 terminology so a direct diff against ffprobe
    // -show_streams or a JM trace's SPS section is straightforward.
    std::uint32_t width = 0, height = 0;
    vsa_h264_sps_picture_size(&sps, &width, &height);

    std::printf("profile_idc=%u\n",                        sps.profile_idc);
    std::printf("constraint_set0_flag=%u\n",               sps.constraint_set0_flag);
    std::printf("constraint_set1_flag=%u\n",               sps.constraint_set1_flag);
    std::printf("constraint_set2_flag=%u\n",               sps.constraint_set2_flag);
    std::printf("constraint_set3_flag=%u\n",               sps.constraint_set3_flag);
    std::printf("constraint_set4_flag=%u\n",               sps.constraint_set4_flag);
    std::printf("constraint_set5_flag=%u\n",               sps.constraint_set5_flag);
    std::printf("level_idc=%u\n",                          sps.level_idc);
    std::printf("seq_parameter_set_id=%u\n",               sps.seq_parameter_set_id);
    std::printf("chroma_format_idc=%u\n",                  sps.chroma_format_idc);
    std::printf("bit_depth_luma_minus8=%u\n",              sps.bit_depth_luma_minus8);
    std::printf("bit_depth_chroma_minus8=%u\n",            sps.bit_depth_chroma_minus8);
    std::printf("log2_max_frame_num_minus4=%u\n",          sps.log2_max_frame_num_minus4);
    std::printf("pic_order_cnt_type=%u\n",                 sps.pic_order_cnt_type);
    std::printf("log2_max_pic_order_cnt_lsb_minus4=%u\n",  sps.log2_max_pic_order_cnt_lsb_minus4);
    std::printf("max_num_ref_frames=%u\n",                 sps.max_num_ref_frames);
    std::printf("gaps_in_frame_num_value_allowed_flag=%u\n", sps.gaps_in_frame_num_value_allowed_flag);
    std::printf("pic_width_in_mbs_minus1=%u\n",            sps.pic_width_in_mbs_minus1);
    std::printf("pic_height_in_map_units_minus1=%u\n",     sps.pic_height_in_map_units_minus1);
    std::printf("frame_mbs_only_flag=%u\n",                sps.frame_mbs_only_flag);
    std::printf("mb_adaptive_frame_field_flag=%u\n",       sps.mb_adaptive_frame_field_flag);
    std::printf("direct_8x8_inference_flag=%u\n",          sps.direct_8x8_inference_flag);
    std::printf("frame_cropping_flag=%u\n",                sps.frame_cropping_flag);
    std::printf("frame_crop_left_offset=%u\n",             sps.frame_crop_left_offset);
    std::printf("frame_crop_right_offset=%u\n",            sps.frame_crop_right_offset);
    std::printf("frame_crop_top_offset=%u\n",              sps.frame_crop_top_offset);
    std::printf("frame_crop_bottom_offset=%u\n",           sps.frame_crop_bottom_offset);
    std::printf("vui_parameters_present_flag=%u\n",        sps.vui_parameters_present_flag);
    // Derived friendly values at the bottom so a reviewer can
    // sanity-check at a glance without recomputing by hand.
    std::printf("pic_width=%u\n",                          width);
    std::printf("pic_height=%u\n",                         height);
}

void dump_slices(const std::vector<std::uint8_t>&   data,
                 const std::vector<vsa_nal_span>&   spans,
                 const vsa_h264_sps&                sps) {
    // One line per slice NAL. Columns:
    //     idx  offset  nal_type  slice_type  type_letter  frame_num
    //     field_pic  idr_id  poc_lsb
    std::size_t slice_idx = 0;
    std::printf("# idx  offset  nal_type  slice_type  type  frame_num  field  idr_id  poc_lsb\n");
    for (const auto& s : spans) {
        if (s.size == 0) continue;
        const std::uint8_t hdr = data[s.offset];
        const std::uint8_t nal_type = static_cast<std::uint8_t>(hdr & 0x1Fu);
        if (nal_type != VSA_H264_NAL_SLICE_NON_IDR
            && nal_type != VSA_H264_NAL_SLICE_IDR) {
            continue;
        }
        const auto rbsp = nal_rbsp(data.data(), s);
        vsa_h264_slice_header sh{};
        const int rc = vsa_h264_parse_slice_header(
            rbsp.data(), rbsp.size(), nal_type, &sps, &sh);
        if (rc != 0) {
            std::fprintf(stderr,
                "vsa_parse: slice header parse error at NAL offset %zu (rc=%d)\n",
                s.offset, rc);
            std::exit(6);
        }
        std::printf("%4zu  %7zu  %2u  %2u  %c  %10u  %u  %u  %u\n",
                    slice_idx,
                    s.offset,
                    nal_type,
                    sh.slice_type,
                    vsa_h264_slice_type_letter(sh.slice_type),
                    sh.frame_num,
                    sh.field_pic_flag,
                    sh.has_idr_pic_id ? sh.idr_pic_id : 0u,
                    sh.has_pic_order_cnt_lsb ? sh.pic_order_cnt_lsb : 0u);
        ++slice_idx;
    }
}

void dump_json(const std::vector<std::uint8_t>&   data,
               const std::vector<vsa_nal_span>&   spans,
               const vsa_h264_sps&                sps) {
    // Compact one-SPS / N-frames summary. Hand-written JSON so we
    // avoid pulling in nlohmann/json just for this CLI.
    std::uint32_t w = 0, h = 0;
    vsa_h264_sps_picture_size(&sps, &w, &h);

    std::printf("{\n");
    std::printf("  \"sps\": {\n");
    std::printf("    \"profile_idc\": %u,\n",    sps.profile_idc);
    std::printf("    \"level_idc\": %u,\n",      sps.level_idc);
    std::printf("    \"chroma_format_idc\": %u,\n", sps.chroma_format_idc);
    std::printf("    \"pic_width\": %u,\n",      w);
    std::printf("    \"pic_height\": %u,\n",     h);
    std::printf("    \"frame_mbs_only_flag\": %u,\n", sps.frame_mbs_only_flag);
    std::printf("    \"pic_order_cnt_type\": %u\n",   sps.pic_order_cnt_type);
    std::printf("  },\n");

    std::size_t total_nal = 0;
    std::size_t n_sps = 0, n_pps = 0, n_idr = 0, n_slice = 0;
    for (const auto& s : spans) {
        if (s.size == 0) continue;
        ++total_nal;
        const std::uint8_t t =
            static_cast<std::uint8_t>(data[s.offset] & 0x1Fu);
        switch (t) {
            case VSA_H264_NAL_SPS:       ++n_sps;   break;
            case VSA_H264_NAL_PPS:       ++n_pps;   break;
            case VSA_H264_NAL_SLICE_IDR: ++n_idr;   break;
            case VSA_H264_NAL_SLICE_NON_IDR: ++n_slice; break;
            default: break;
        }
    }
    std::printf("  \"nal_count\": %zu,\n",  total_nal);
    std::printf("  \"sps_count\": %zu,\n",  n_sps);
    std::printf("  \"pps_count\": %zu,\n",  n_pps);
    std::printf("  \"idr_count\": %zu,\n",  n_idr);
    std::printf("  \"non_idr_slice_count\": %zu,\n", n_slice);

    // Per-frame summary: one entry per slice NAL, reusing the slice
    // header parser. Any parse error is fatal.
    std::printf("  \"frames\": [\n");
    bool first_frame = true;
    for (const auto& s : spans) {
        if (s.size == 0) continue;
        const std::uint8_t t =
            static_cast<std::uint8_t>(data[s.offset] & 0x1Fu);
        if (t != VSA_H264_NAL_SLICE_IDR && t != VSA_H264_NAL_SLICE_NON_IDR) {
            continue;
        }
        const auto rbsp = nal_rbsp(data.data(), s);
        vsa_h264_slice_header sh{};
        const int rc = vsa_h264_parse_slice_header(
            rbsp.data(), rbsp.size(), t, &sps, &sh);
        if (rc != 0) {
            std::fprintf(stderr,
                "vsa_parse: slice header parse error at NAL offset %zu (rc=%d)\n",
                s.offset, rc);
            std::exit(6);
        }
        if (!first_frame) std::printf(",\n");
        first_frame = false;
        const char letter = vsa_h264_slice_type_letter(sh.slice_type);
        std::printf("    {\"offset\": %zu, \"size\": %zu, "
                    "\"nal_type\": %u, \"slice_type\": %u, "
                    "\"type\": \"%c\", \"frame_num\": %u}",
                    s.offset, s.size, t, sh.slice_type, letter,
                    sh.frame_num);
    }
    if (!first_frame) std::printf("\n");
    std::printf("  ]\n");
    std::printf("}\n");
}

int run_with_file(const char* path, Mode mode) {
    auto data = read_file(path);

    // First pass: count, then size the span vector, then scan again.
    const std::size_t nal_total =
        vsa_annexb_find_nals(data.data(), data.size(), nullptr, 0);
    if (nal_total == 0) {
        std::fprintf(stderr, "vsa_parse: no NAL units found in %s\n", path);
        return 4;
    }
    std::vector<vsa_nal_span> spans(nal_total);
    const std::size_t got = vsa_annexb_find_nals(
        data.data(), data.size(), spans.data(), spans.size());
    spans.resize(got);

    const auto sps_result = find_and_parse_sps(data, spans);
    if (!sps_result.ok) {
        return 5;
    }

    switch (mode) {
        case Mode::kDumpSps:    dump_sps(sps_result.sps);                       break;
        case Mode::kDumpSlices: dump_slices(data, spans, sps_result.sps);       break;
        case Mode::kJson:       dump_json(data, spans, sps_result.sps);         break;
        default: break;
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 0;
    }

    // `--version` is a single-flag mode with no file argument.
    if (argc == 2 && std::strcmp(argv[1], "--version") == 0) {
        std::printf("%s\n", vsa_codec_version());
        return 0;
    }

    if (argc != 3) {
        print_usage(argv[0]);
        return 2;
    }

    const char* path = argv[1];
    const char* flag = argv[2];
    Mode mode = Mode::kNone;
    if      (std::strcmp(flag, "--dump-sps")    == 0) mode = Mode::kDumpSps;
    else if (std::strcmp(flag, "--dump-slices") == 0) mode = Mode::kDumpSlices;
    else if (std::strcmp(flag, "--json")        == 0) mode = Mode::kJson;
    else {
        print_usage(argv[0]);
        return 2;
    }

    return run_with_file(path, mode);
}
