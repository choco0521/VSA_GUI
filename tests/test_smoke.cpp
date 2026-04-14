// Smoke test: ensure the pure-C codec library compiles, links into the
// C++ test binary, and is callable across the extern "C" boundary.
#include <doctest/doctest.h>

extern "C" {
#include "codec/codec_version.h"
}

#include <cstring>

TEST_CASE("vsa_codec_version returns a non-empty string") {
    const char* v = vsa_codec_version();
    REQUIRE(v != nullptr);
    CHECK(std::strlen(v) > 0);
}
