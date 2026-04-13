#ifndef VSA_CODEC_VERSION_H
#define VSA_CODEC_VERSION_H

#ifdef __cplusplus
extern "C" {
#endif

/* Returns the semantic version of the vsa_codec library as a C string.
 * The pointer is valid for the lifetime of the program. This function
 * exists primarily as a smoke-test symbol so the C library, its linkage
 * to C++ consumers, and the CI pipeline can all be validated before any
 * real parser code is introduced. */
const char* vsa_codec_version(void);

#ifdef __cplusplus
}
#endif

#endif /* VSA_CODEC_VERSION_H */
