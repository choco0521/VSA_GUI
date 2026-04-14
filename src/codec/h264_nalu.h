#ifndef VSA_CODEC_H264_NALU_H
#define VSA_CODEC_H264_NALU_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * H.264 NAL unit types (ISO/IEC 14496-10 §7.4.1, Table 7-1).
 *
 * Only the codes the VSA parser actually handles today are named
 * explicitly. The enum is deliberately not sparse — downstream code
 * should mask `nal_header & 0x1F` and compare against these values
 * directly; any unknown type is just a plain integer.
 */
enum vsa_h264_nal_type {
    VSA_H264_NAL_UNSPECIFIED       = 0,
    VSA_H264_NAL_SLICE_NON_IDR     = 1,
    VSA_H264_NAL_DPA               = 2,
    VSA_H264_NAL_DPB               = 3,
    VSA_H264_NAL_DPC               = 4,
    VSA_H264_NAL_SLICE_IDR         = 5,
    VSA_H264_NAL_SEI               = 6,
    VSA_H264_NAL_SPS               = 7,
    VSA_H264_NAL_PPS               = 8,
    VSA_H264_NAL_AUD               = 9,
    VSA_H264_NAL_END_OF_SEQUENCE   = 10,
    VSA_H264_NAL_END_OF_STREAM     = 11,
    VSA_H264_NAL_FILLER            = 12,
    VSA_H264_NAL_SPS_EXTENSION     = 13,
    VSA_H264_NAL_PREFIX            = 14,
    VSA_H264_NAL_SUBSET_SPS        = 15,
    VSA_H264_NAL_AUX_SLICE         = 19,
    VSA_H264_NAL_SLICE_EXT         = 20
};

#ifdef __cplusplus
}
#endif

#endif /* VSA_CODEC_H264_NALU_H */
