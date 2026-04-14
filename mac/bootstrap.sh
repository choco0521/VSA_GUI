#!/usr/bin/env bash
#
# bootstrap.sh — one-shot macOS dev-environment setup for VSAnalyzer.
#
# Run this once on a fresh Mac before the first `swift build`:
#
#     cd VSA_GUI/mac
#     ./bootstrap.sh
#
# It is idempotent — re-running after the env is already set up just
# reports "already installed" for every component and exits cleanly.
#
# What it sets up:
#
#   1. Homebrew              (if not present)
#   2. Xcode Command Line    (via `xcode-select --install` prompt
#      Tools                  or recommendation to run it manually)
#   3. Swift 5.9+            (bundled with Xcode 15 / CLT)
#   4. ffmpeg                (for vsa_extract.sh and optional
#                              sample-clip regeneration)
#   5. cmake + ninja         (for the legacy Qt/CMake build, if the
#                              user wants to also build the Linux/
#                              Qt target on their Mac)
#   6. Qt 6                  (optional — only if the user wants to
#                              build the Qt vsa_gui target)
#
# The script deliberately does NOT install clang-format, doctest or
# any other non-essential tooling. That stays opt-in.

set -euo pipefail

BLUE='\033[1;34m'
GREEN='\033[1;32m'
YELLOW='\033[1;33m'
RED='\033[1;31m'
RESET='\033[0m'

log()   { printf "${BLUE}==>${RESET} %s\n" "$*"; }
ok()    { printf "${GREEN}✓${RESET}  %s\n" "$*"; }
warn()  { printf "${YELLOW}!${RESET}  %s\n" "$*" >&2; }
fail()  { printf "${RED}✗${RESET}  %s\n" "$*" >&2; exit 1; }

# ---------------------------------------------------------------------
# Step 0: verify we are on macOS. SwiftPM / AppKit / Metal are
# macOS-only. Running this on Linux or WSL will fail loudly.
# ---------------------------------------------------------------------
if [[ "$(uname -s)" != "Darwin" ]]; then
    fail "bootstrap.sh only supports macOS. On Linux use the CMake/Qt build at the repo root."
fi

log "Detected macOS $(sw_vers -productVersion) on $(uname -m)"

# ---------------------------------------------------------------------
# Step 1: Xcode Command Line Tools. These ship clang, the macOS
# SDK, and the standalone `swift` compiler. If they are missing we
# trigger Apple's own installer dialog and bail so the user can
# re-run bootstrap.sh after the install finishes.
# ---------------------------------------------------------------------
if ! xcode-select -p >/dev/null 2>&1; then
    log "Xcode Command Line Tools are missing — triggering installer"
    xcode-select --install || true
    fail "Re-run bootstrap.sh after the Command Line Tools installer finishes."
fi
ok "Xcode Command Line Tools present ($(xcode-select -p))"

# ---------------------------------------------------------------------
# Step 2: Swift. On a fully installed CLT or Xcode this is
# automatic; we still verify the version because SwiftPM
# `swift-tools-version:5.9` requires 5.9 or newer.
# ---------------------------------------------------------------------
if ! command -v swift >/dev/null 2>&1; then
    fail "swift binary not found on PATH — install Xcode 15 or the latest CLT."
fi

swift_version="$(swift --version 2>&1 | head -1)"
log "$swift_version"
# Parse the leading version tuple. Swift's first line looks like:
#   Apple Swift version 5.9.2 (swiftlang-5.9.2.1.1 clang-1500.0.40.1)
swift_major="$(echo "$swift_version" | sed -nE 's/.*Apple Swift version ([0-9]+)\.([0-9]+).*/\1/p')"
swift_minor="$(echo "$swift_version" | sed -nE 's/.*Apple Swift version ([0-9]+)\.([0-9]+).*/\2/p')"
if [[ -z "$swift_major" ]]; then
    warn "could not parse Swift version, continuing anyway"
elif [[ "$swift_major" -lt 5 ]] \
     || { [[ "$swift_major" -eq 5 ]] && [[ "$swift_minor" -lt 9 ]]; }; then
    fail "Swift $swift_major.$swift_minor is too old — need 5.9+. Upgrade Xcode or install a newer Swift toolchain from https://swift.org."
fi
ok "Swift $swift_major.$swift_minor meets the 5.9+ requirement"

# ---------------------------------------------------------------------
# Step 3: Homebrew. Needed for ffmpeg / cmake / Qt. Homebrew itself
# is optional if the user already has these via MacPorts or
# manually, but we default-install it for simplicity.
# ---------------------------------------------------------------------
if ! command -v brew >/dev/null 2>&1; then
    log "Homebrew not found — installing it"
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
fi
if command -v brew >/dev/null 2>&1; then
    ok "Homebrew: $(brew --prefix)"
else
    warn "Homebrew install did not succeed, subsequent steps may be skipped"
fi

# Helper: install a package via brew if it's not already present.
brew_ensure() {
    local formula="$1"
    local binary="${2:-$1}"
    if command -v "$binary" >/dev/null 2>&1; then
        ok "$formula already installed ($(command -v "$binary"))"
        return 0
    fi
    if command -v brew >/dev/null 2>&1; then
        log "brew install $formula"
        brew install "$formula"
    else
        warn "cannot install $formula (brew missing) — please install manually"
    fi
}

# ---------------------------------------------------------------------
# Step 4: ffmpeg — used by tests/scripts/vsa_extract.sh and by the
# sample-clip regeneration in tests/fixtures/*. Not strictly
# required for `swift build`, but the Phase G pipeline depends on
# it.
# ---------------------------------------------------------------------
brew_ensure ffmpeg ffmpeg

# ---------------------------------------------------------------------
# Step 5: cmake + ninja — only needed if the user also wants to
# build the Linux/Qt vsa_gui target on their Mac. Small download
# so we install them unconditionally.
# ---------------------------------------------------------------------
brew_ensure cmake cmake
brew_ensure ninja ninja

# ---------------------------------------------------------------------
# Step 6: Qt 6 — OPTIONAL. The Qt target only exists for CI / Linux
# development parity. macOS users who only care about the SwiftPM
# VSAnalyzer target can skip it. We offer a yes/no prompt.
# ---------------------------------------------------------------------
if brew list --formula 2>/dev/null | grep -q '^qt$'; then
    ok "Qt already installed"
else
    read -r -p "Install Qt 6 so you can also build the CMake/Qt vsa_gui target? [y/N] " qt_reply < /dev/tty || qt_reply="n"
    if [[ "$qt_reply" =~ ^[Yy]$ ]]; then
        brew_ensure qt qmake
    else
        warn "Skipping Qt — SwiftPM VSAnalyzer build does not need it"
    fi
fi

# ---------------------------------------------------------------------
# Step 7: smoke-test the SwiftPM build. This catches environment
# problems immediately instead of on the first manual `swift build`.
# ---------------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

log "Resolving SwiftPM dependencies"
swift package resolve

log "Running swift build (Debug)"
swift build

ok "Phase I VSAnalyzer SwiftPM build succeeded"

cat <<EOF

${GREEN}Setup complete.${RESET}

Next steps:

    cd $(pwd)
    swift run VSAnalyzer                  # launches the SwiftUI app
    open \$(swift build --show-bin-path)/VSAnalyzer

When a sample file is needed:

    ../tests/fixtures/tiny_clip.h264      # already in the repo, 4870 bytes

CMake / Qt (optional, Linux parity):

    cd ..
    cmake -S . -B build -G Ninja \\
        -DCMAKE_PREFIX_PATH="\$(brew --prefix qt)" \\
        -DCMAKE_BUILD_TYPE=Release
    cmake --build build -j
    ctest --test-dir build --output-on-failure

Report any issues to the same Claude Code session that generated this
script. The sandbox pushes fixes via git; pull them with:

    git pull --ff-only origin claude/organize-progress-report-q2VAL
EOF
