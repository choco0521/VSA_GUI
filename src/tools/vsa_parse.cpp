// vsa_parse — headless CLI that exercises the vsa_codec C library from C++.
//
// Phase A: stub only. It reports the codec version and exits. Later phases
// will add --dump-sps / --dump-slices / --json modes that run the real
// H.264 parser and emit text that is diffed against the JM reference
// decoder's trace output in CI.

extern "C" {
#include "codec/codec_version.h"
}

#include <cstdio>
#include <cstring>

namespace {

void print_usage(const char* argv0) {
    std::fprintf(stderr,
        "usage: %s [--version]\n"
        "\n"
        "H.264 parser driver (scaffold). Real modes (--dump-sps,\n"
        "--dump-slices, --json) will be added in later phases.\n",
        argv0);
}

} // namespace

int main(int argc, char** argv) {
    if (argc >= 2 && std::strcmp(argv[1], "--version") == 0) {
        std::printf("%s\n", vsa_codec_version());
        return 0;
    }
    print_usage(argv[0]);
    return argc == 1 ? 0 : 2;
}
