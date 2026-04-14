/*
 * VSACodec.h — Umbrella header exposing every public entry of the
 * pure-C vsa_codec library to Swift via SwiftPM's module map.
 *
 * Usage from Swift:
 *
 *     import VSACodec
 *
 *     let version = String(cString: vsa_codec_version())
 *
 * The underlying C sources live at ../../../src/codec (reached via
 * the `codec` symlink next to this include directory) and are
 * compiled as C11 by the SwiftPM build. Every symbol declared here
 * is guarded with `extern "C"` in the source headers so consumers
 * get plain C linkage regardless of how the file is included.
 *
 * This header is the ONLY file Swift code should see from the C
 * library. Keeping it minimal and curated means adding a new parser
 * module to src/codec does not break the Swift build unless it is
 * also added to this umbrella.
 */
#ifndef VSA_VSACODEC_UMBRELLA_H
#define VSA_VSACODEC_UMBRELLA_H

#include "../codec/codec_version.h"
#include "../codec/bit_reader.h"
#include "../codec/annexb.h"
#include "../codec/rbsp.h"
#include "../codec/h264_nalu.h"
#include "../codec/h264_sps.h"
#include "../codec/h264_slice_header.h"

#endif /* VSA_VSACODEC_UMBRELLA_H */
