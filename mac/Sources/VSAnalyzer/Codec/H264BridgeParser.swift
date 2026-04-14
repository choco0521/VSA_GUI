//
//  H264BridgeParser.swift
//
//  The Swift-side wrapper around the C11 vsa_codec library. Every
//  unsafe/extern-C ugly is confined to this file; callers elsewhere
//  in the app get a clean `parse(path:) throws -> StreamSummary`
//  that either returns a populated value or throws a typed error.
//
//  This mirrors the Qt-side src/model/H264DataProvider.{h,cpp} so
//  the macOS native build consumes the exact same parser surface
//  that the Linux CI build does, just with Swift types on top.
//

import Foundation

import VSACodec

/// All H.264 parser interactions for the Phase I UI.
struct H264BridgeParser {
    enum ParseError: Error, CustomStringConvertible {
        case fileNotFound(String)
        case ioError(String)
        case emptyFile
        case noNalUnits
        case spsNotFound
        case spsParseError(Int32)
        case invalidPictureSize
        case noDecodableSlices

        var description: String {
            switch self {
            case .fileNotFound(let p):  return "File not found: \(p)"
            case .ioError(let m):       return "I/O error: \(m)"
            case .emptyFile:            return "File is empty"
            case .noNalUnits:           return "No NAL units found (not a raw H.264 Annex B stream?)"
            case .spsNotFound:          return "No parseable SPS NAL unit found"
            case .spsParseError(let r): return "SPS parse error (rc=\(r))"
            case .invalidPictureSize:   return "SPS describes an unrepresentable picture size"
            case .noDecodableSlices:    return "Stream had an SPS but no decodable slices"
            }
        }
    }

    /// Human-readable version string from the C library. Useful for
    /// the About dialog and for sanity-checking the Swift ↔ C link.
    var version: String {
        String(cString: vsa_codec_version())
    }

    /// Parse an Annex B H.264 file into a StreamSummary value.
    func parse(path: String) throws -> StreamSummary {
        guard FileManager.default.fileExists(atPath: path) else {
            throw ParseError.fileNotFound(path)
        }
        let url = URL(fileURLWithPath: path)

        let data: Data
        do {
            data = try Data(contentsOf: url, options: .mappedIfSafe)
        } catch {
            throw ParseError.ioError(error.localizedDescription)
        }
        if data.isEmpty {
            throw ParseError.emptyFile
        }

        let fileSize = UInt64(data.count)

        // The Data buffer can be non-contiguous in principle but
        // .mappedIfSafe gives us a contiguous backing store, so
        // withUnsafeBytes is cheap and safe.
        return try data.withUnsafeBytes { raw -> StreamSummary in
            guard let base = raw.baseAddress?.assumingMemoryBound(to: UInt8.self) else {
                throw ParseError.emptyFile
            }
            let size = raw.count

            // ---- First pass: count NAL units ----
            let nalTotal = vsa_annexb_find_nals(base, size, nil, 0)
            if nalTotal == 0 { throw ParseError.noNalUnits }

            // ---- Second pass: record every span ----
            var spans = [vsa_nal_span](repeating: vsa_nal_span(offset: 0, size: 0),
                                       count: nalTotal)
            let written = spans.withUnsafeMutableBufferPointer { buf -> Int in
                Int(vsa_annexb_find_nals(base, size, buf.baseAddress, nalTotal))
            }
            spans.removeSubrange(written..<spans.count)

            // ---- Locate first SPS ----
            var spsParsed = false
            var sps = vsa_h264_sps()
            for span in spans where span.size >= 1 {
                let nalType = base[span.offset] & 0x1F
                if Int32(nalType) != VSA_H264_NAL_SPS.rawValue { continue }
                // Strip the NAL header byte, run RBSP unescape.
                let payloadStart = span.offset + 1
                let payloadLen   = Int(span.size) - 1
                var rbsp = [UInt8](repeating: 0, count: payloadLen)
                let rbspLen = rbsp.withUnsafeMutableBufferPointer { rb -> Int in
                    Int(vsa_rbsp_unescape(base + payloadStart,
                                          payloadLen,
                                          rb.baseAddress))
                }
                let rc = rbsp.withUnsafeBufferPointer { rb -> Int32 in
                    vsa_h264_parse_sps(rb.baseAddress, rbspLen, &sps)
                }
                if rc != 0 {
                    throw ParseError.spsParseError(rc)
                }
                spsParsed = true
                break
            }
            if !spsParsed { throw ParseError.spsNotFound }

            // ---- Derive picture size ----
            var width: UInt32 = 0
            var height: UInt32 = 0
            let sizeRc = vsa_h264_sps_picture_size(&sps, &width, &height)
            if sizeRc != 0 { throw ParseError.invalidPictureSize }

            // ---- Tally NAL types for the summary row ----
            var nalCount = 0
            var spsCount = 0
            var ppsCount = 0
            var idrCount = 0
            var nonIdrSliceCount = 0
            var seiCount = 0
            var audCount = 0
            for span in spans where span.size > 0 {
                nalCount += 1
                let t = Int32(base[span.offset] & 0x1F)
                switch t {
                case VSA_H264_NAL_SPS.rawValue:           spsCount += 1
                case VSA_H264_NAL_PPS.rawValue:           ppsCount += 1
                case VSA_H264_NAL_SLICE_IDR.rawValue:     idrCount += 1
                case VSA_H264_NAL_SLICE_NON_IDR.rawValue: nonIdrSliceCount += 1
                case VSA_H264_NAL_SEI.rawValue:           seiCount += 1
                case VSA_H264_NAL_AUD.rawValue:           audCount += 1
                default: break
                }
            }

            if idrCount == 0 && nonIdrSliceCount == 0 {
                throw ParseError.noDecodableSlices
            }

            return StreamSummary(
                filePath: path,
                fileSizeBytes: fileSize,
                profileName: Self.profileName(profileIdc: sps.profile_idc,
                                              constraintSet1: sps.constraint_set1_flag),
                levelString: String(format: "%d.%d", sps.level_idc / 10, sps.level_idc % 10),
                chromaFormat: Self.chromaName(sps.chroma_format_idc),
                bitDepthLuma: Int(sps.bit_depth_luma_minus8) + 8,
                pictureWidth: Int(width),
                pictureHeight: Int(height),
                frameMbsOnly: sps.frame_mbs_only_flag != 0,
                picOrderCntType: Int(sps.pic_order_cnt_type),
                picWidthInMbs: Int(sps.pic_width_in_mbs_minus1) + 1,
                picHeightInMapUnits: Int(sps.pic_height_in_map_units_minus1) + 1,
                vuiPresent: sps.vui_parameters_present_flag != 0,
                nalCount: nalCount,
                spsCount: spsCount,
                ppsCount: ppsCount,
                idrCount: idrCount,
                nonIdrSliceCount: nonIdrSliceCount,
                seiCount: seiCount,
                audCount: audCount
            )
        }
    }

    // --------------------------------------------------------------
    // Display helpers (value → human string). Kept here rather than
    // on the StreamSummary struct so the struct stays a pure value.
    // --------------------------------------------------------------

    private static func profileName(profileIdc: UInt8, constraintSet1: UInt8) -> String {
        switch profileIdc {
        case 66:  return constraintSet1 != 0 ? "Constrained Baseline" : "Baseline"
        case 77:  return "Main"
        case 88:  return "Extended"
        case 100: return "High"
        case 110: return "High 10"
        case 122: return "High 4:2:2"
        case 244: return "High 4:4:4 Predictive"
        default:  return "profile_idc=\(profileIdc)"
        }
    }

    private static func chromaName(_ value: UInt32) -> String {
        switch value {
        case 0:  return "monochrome"
        case 1:  return "4:2:0"
        case 2:  return "4:2:2"
        case 3:  return "4:4:4"
        default: return "reserved (\(value))"
        }
    }
}
