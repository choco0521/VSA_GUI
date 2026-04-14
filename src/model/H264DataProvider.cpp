#include "H264DataProvider.h"

extern "C" {
#include "codec/annexb.h"
#include "codec/h264_nalu.h"
#include "codec/h264_slice_header.h"
#include "codec/h264_sps.h"
#include "codec/rbsp.h"
}

#include <QFile>
#include <QFileInfo>

#include <climits>
#include <cstdint>
#include <vector>

namespace vsa {

namespace {

// Strip the NAL header byte and run vsa_rbsp_unescape on the rest.
// Returns the emulation-free RBSP payload the codec headers want.
std::vector<std::uint8_t> nal_to_rbsp(const std::uint8_t* buf,
                                       const vsa_nal_span& span) {
    if (span.size < 1u) {
        return {};
    }
    const std::uint8_t* in      = buf + span.offset + 1;
    const std::size_t   in_size = span.size - 1u;
    std::vector<std::uint8_t> rbsp(in_size);
    const std::size_t rbsp_size = vsa_rbsp_unescape(in, in_size, rbsp.data());
    rbsp.resize(rbsp_size);
    return rbsp;
}

QString profile_name(std::uint8_t profile_idc,
                     std::uint8_t constraint_set1_flag) {
    // The handful of profiles the VSA GUI cares about, plus a
    // generic fallback that shows the raw profile_idc for anything
    // else so nothing is silently mis-labelled.
    switch (profile_idc) {
        case 66:
            return constraint_set1_flag ? QStringLiteral("Constrained Baseline")
                                        : QStringLiteral("Baseline");
        case 77:  return QStringLiteral("Main");
        case 88:  return QStringLiteral("Extended");
        case 100: return QStringLiteral("High");
        case 110: return QStringLiteral("High 10");
        case 122: return QStringLiteral("High 4:2:2");
        case 244: return QStringLiteral("High 4:4:4 Predictive");
        default:  return QStringLiteral("profile_idc=%1").arg(profile_idc);
    }
}

QString chroma_name(std::uint32_t chroma_format_idc) {
    switch (chroma_format_idc) {
        case 0:  return QStringLiteral("monochrome");
        case 1:  return QStringLiteral("4:2:0");
        case 2:  return QStringLiteral("4:2:2");
        case 3:  return QStringLiteral("4:4:4");
        default: return QStringLiteral("reserved (%1)").arg(chroma_format_idc);
    }
}

} // namespace

H264DataProvider::H264DataProvider(const QString& filePath) {
    parse(filePath);
}

void H264DataProvider::parse(const QString& filePath) {
    m_valid = false;
    m_error.clear();
    m_frames.clear();
    m_pictureWidth  = 0;
    m_pictureHeight = 0;

    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) {
        m_error = QStringLiteral("cannot open file: %1").arg(f.errorString());
        return;
    }
    const QByteArray raw = f.readAll();
    f.close();
    if (raw.isEmpty()) {
        m_error = QStringLiteral("file is empty");
        return;
    }

    const auto* const data =
        reinterpret_cast<const std::uint8_t*>(raw.constData());
    const std::size_t size = static_cast<std::size_t>(raw.size());

    // Two-pass NAL scan: count first, then allocate a vector the right
    // size and record every span.
    const std::size_t nal_total =
        vsa_annexb_find_nals(data, size, nullptr, 0);
    if (nal_total == 0u) {
        m_error = QStringLiteral("no NAL units found (not a raw H.264 Annex B stream?)");
        return;
    }
    std::vector<vsa_nal_span> spans(nal_total);
    const std::size_t got =
        vsa_annexb_find_nals(data, size, spans.data(), spans.size());
    spans.resize(got);

    // -----------------------------------------------------------------
    // SPS — take the first one we see. Every GOP in tiny_clip / Elecard
    // TheaterSquare repeats the SPS so a single-SPS view is enough for
    // the Stream Info tree; streams that mid-stream-switch resolution
    // are out of scope here.
    // -----------------------------------------------------------------
    vsa_h264_sps sps{};
    bool sps_ok = false;
    for (const auto& span : spans) {
        if (span.size == 0u) continue;
        const std::uint8_t nal_type =
            static_cast<std::uint8_t>(data[span.offset] & 0x1Fu);
        if (nal_type != VSA_H264_NAL_SPS) continue;
        const auto rbsp = nal_to_rbsp(data, span);
        if (vsa_h264_parse_sps(rbsp.data(), rbsp.size(), &sps) == 0) {
            sps_ok = true;
            break;
        }
    }
    if (!sps_ok) {
        m_error = QStringLiteral("no parseable SPS NAL found");
        return;
    }

    // Derive picture dimensions, honouring frame_cropping_flag.
    std::uint32_t w = 0;
    std::uint32_t h = 0;
    if (vsa_h264_sps_picture_size(&sps, &w, &h) != 0) {
        m_error = QStringLiteral("SPS describes an unrepresentable picture size");
        return;
    }
    m_pictureWidth  = static_cast<int>(w);
    m_pictureHeight = static_cast<int>(h);

    // -----------------------------------------------------------------
    // Build StreamInfo. Fields we can populate from SPS alone go in;
    // the rest keep their placeholder defaults from the struct so the
    // Stream Info tree still renders every row.
    // -----------------------------------------------------------------
    m_stream = StreamInfo{};
    m_stream.streamType   = QStringLiteral("AVC / H.264");
    m_stream.profile      = profile_name(sps.profile_idc, sps.constraint_set1_flag);
    m_stream.compatibility = m_stream.profile;
    m_stream.levelTier    = QStringLiteral("%1.%2")
                                .arg(sps.level_idc / 10)
                                .arg(sps.level_idc % 10);
    m_stream.chromaFormat = chroma_name(sps.chroma_format_idc);
    m_stream.bitDepth     = static_cast<int>(sps.bit_depth_luma_minus8) + 8;
    m_stream.resolution   = QStringLiteral("%1 x %2").arg(m_pictureWidth).arg(m_pictureHeight);
    m_stream.declaredBitrate = QStringLiteral("Undefined");
    // frameRate, duration and muxDuration cannot be derived from an
    // elementary stream alone — they depend on the container (or the
    // VUI timing info, which we do not yet parse).
    m_stream.frameRate    = 0.0;
    m_stream.duration     = QStringLiteral("(unknown — no container)");
    m_stream.muxDuration  = m_stream.duration;
    m_stream.psnrYUV      = QStringLiteral("—");

    // -----------------------------------------------------------------
    // Slice pass: every VSA_H264_NAL_SLICE_IDR / NAL_SLICE_NON_IDR in
    // the stream becomes one FrameData entry. poc is assigned as a
    // monotonic index rather than the spec-mandated POC value — we
    // cannot compute the latter without PPS and the full reference
    // picture marking path, and for chart / table display a
    // decode-order index is equivalent.
    // -----------------------------------------------------------------
    m_frames.reserve(static_cast<int>(spans.size()));
    int slice_idx  = 0;
    int i_count    = 0;
    int p_count    = 0;
    int b_count    = 0;
    qint64 bit_total = 0;
    int size_min = INT_MAX;
    int size_max = 0;

    for (const auto& span : spans) {
        if (span.size == 0u) continue;
        const std::uint8_t hdr      = data[span.offset];
        const std::uint8_t nal_type = static_cast<std::uint8_t>(hdr & 0x1Fu);
        if (nal_type != VSA_H264_NAL_SLICE_IDR
            && nal_type != VSA_H264_NAL_SLICE_NON_IDR) {
            continue;
        }

        const auto rbsp = nal_to_rbsp(data, span);
        vsa_h264_slice_header sh{};
        if (vsa_h264_parse_slice_header(
                rbsp.data(), rbsp.size(), nal_type, &sps, &sh) != 0) {
            // Keep the frame with a '?' type instead of failing the
            // whole file. Real-world streams occasionally carry
            // partial NALs at the tail.
            FrameData fd;
            fd.poc        = slice_idx;
            fd.streamIdx  = slice_idx;
            fd.displayIdx = slice_idx;
            fd.type       = QStringLiteral("?");
            fd.sizeBytes  = static_cast<int>(span.size);
            fd.offset     = static_cast<qint64>(span.offset);
            m_frames.push_back(fd);
            ++slice_idx;
            continue;
        }

        FrameData fd;
        fd.poc        = slice_idx;
        fd.streamIdx  = slice_idx;
        fd.displayIdx = slice_idx;
        const char letter = vsa_h264_slice_type_letter(sh.slice_type);
        fd.type       = QString(QChar::fromLatin1(letter));
        fd.sizeBytes  = static_cast<int>(span.size);
        fd.offset     = static_cast<qint64>(span.offset);
        fd.isKey      = (nal_type == VSA_H264_NAL_SLICE_IDR);
        fd.show       = true;
        // timeMs assumes a default 30 fps timeline until we parse VUI
        // timing info. The chart still renders correctly because it
        // uses frame index, not wall time.
        fd.timeMs     = static_cast<qint64>(slice_idx) * 33;
        // quant / psnr / CU stats cannot be recovered without CABAC
        // decoding, so they stay at their struct defaults and the
        // Stream Info tree will show the placeholder values.
        m_frames.push_back(fd);

        switch (letter) {
            case 'I': ++i_count; break;
            case 'P': ++p_count; break;
            case 'B': ++b_count; break;
            default: break;
        }
        bit_total += span.size;
        const int isz = static_cast<int>(span.size);
        if (isz < size_min) size_min = isz;
        if (isz > size_max) size_max = isz;
        ++slice_idx;
    }

    if (m_frames.isEmpty()) {
        m_error = QStringLiteral("stream had an SPS but no decodable slices");
        return;
    }

    m_stream.frameCount  = m_frames.size();
    m_stream.iFrames     = i_count;
    m_stream.pFrames     = p_count;
    m_stream.bFrames     = b_count;
    m_stream.bitsPerFrameMin = size_min == INT_MAX ? 0 : size_min;
    m_stream.bitsPerFrameMax = size_max;
    m_stream.bitsPerFrameAvg = m_frames.isEmpty()
        ? 0
        : static_cast<int>(bit_total / m_frames.size());

    m_valid = true;
}

} // namespace vsa
