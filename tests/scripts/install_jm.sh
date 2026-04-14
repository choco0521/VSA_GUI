#!/usr/bin/env bash
#
# install_jm.sh — download, patch and build the JM H.264 reference
# software (Joint Model) with TRACE output enabled, so Phase D.3 can
# diff vsa_parse's SPS / slice header dump against the ISO reference
# implementation byte for byte.
#
# Usage:
#     install_jm.sh <install_dir>
#
# On success produces:
#     <install_dir>/bin/ldecod.exe     (patched, TRACE=1)
#     <install_dir>/bin/lencod.exe     (unpatched, only needed by a
#                                       minority of verification flows)
#     <install_dir>/VERSION            (git rev or tarball filename)
#     <install_dir>/STATUS             (either "ready" or the error)
#
# This script is invoked exclusively from GitHub Actions runners that
# have network access. It cannot run inside the Claude Code sandbox
# because vcgit.hhi.fraunhofer.de / iphome.hhi.de are not on the
# egress allowlist.

set -uo pipefail

INSTALL_DIR="${1:-$HOME/.cache/jm}"
WORK_DIR="$(mktemp -d -t jm-build.XXXXXX)"
trap 'rm -rf "$WORK_DIR"' EXIT

mkdir -p "$INSTALL_DIR" "$INSTALL_DIR/bin"

# ---------------------------------------------------------------------
# Step 1: fetch the source tree.
#
# Canonical upstream: https://vcgit.hhi.fraunhofer.de/jvet/JM.git
# Secondary mirror:   https://github.com/OpenHEVC/JM.git
#                     (community fork kept reasonably in sync)
#
# We try git clone first because it gives us a revision-stamped
# working copy we can log in VERSION; if that fails we fall back to a
# known-good tarball from iphome.hhi.de.
# ---------------------------------------------------------------------
cd "$WORK_DIR"
JM_DIR=""

try_git() {
    local url="$1"
    echo "install_jm.sh: git clone $url"
    if git clone --depth 1 "$url" JM 2>&1 | tail -5; then
        JM_DIR="$WORK_DIR/JM"
        (cd JM && git rev-parse HEAD > "$INSTALL_DIR/VERSION") || true
        return 0
    fi
    return 1
}

try_tarball() {
    local url="$1" name="$2"
    echo "install_jm.sh: curl $url"
    if curl -fsSL --retry 3 --retry-delay 2 "$url" -o "$name"; then
        unzip -q "$name" || tar xf "$name" || return 1
        # The archive usually unpacks into "JM/".
        if [[ -d JM ]]; then
            JM_DIR="$WORK_DIR/JM"
        else
            JM_DIR="$(find . -maxdepth 2 -type d -name 'JM*' | head -n 1 || true)"
        fi
        [[ -n "$JM_DIR" && -d "$JM_DIR" ]] || return 1
        basename "$url" > "$INSTALL_DIR/VERSION"
        return 0
    fi
    return 1
}

if ! try_git https://vcgit.hhi.fraunhofer.de/jvet/JM.git; then
    echo "install_jm.sh: HHI gitlab clone failed, trying OpenHEVC mirror"
    if ! try_git https://github.com/OpenHEVC/JM.git; then
        echo "install_jm.sh: mirror clone failed, trying tarball"
        if ! try_tarball http://iphome.hhi.de/suehring/tml/download/jm19.0.zip jm19.0.zip; then
            echo "install_jm.sh: all JM fetch paths failed" >&2
            echo "fetch-failed" > "$INSTALL_DIR/STATUS"
            exit 2
        fi
    fi
fi

if [[ -z "$JM_DIR" || ! -d "$JM_DIR" ]]; then
    echo "install_jm.sh: could not locate unpacked JM directory" >&2
    echo "layout-error" > "$INSTALL_DIR/STATUS"
    exit 3
fi

# ---------------------------------------------------------------------
# Step 2: enable TRACE output on the decoder.
#
# The source tree ships with `#define TRACE 0` in
# ldecod/inc/defines.h. Flipping it to 1 makes ldecod.exe emit a
# human-readable log of every syntax element it parses, which is what
# Phase D.3 diffs against vsa_parse output.
# ---------------------------------------------------------------------
DEFINES="$JM_DIR/ldecod/inc/defines.h"
if [[ -f "$DEFINES" ]]; then
    echo "install_jm.sh: patching TRACE macro in $DEFINES"
    # The value can be 0, 1 or even an expression like "0 // ..."; any
    # assignment to TRACE gets overwritten.
    sed -i.bak -E 's@^(#define[[:space:]]+TRACE[[:space:]]+)[0-9]+.*$@\11@' "$DEFINES"
    grep -n '#define[[:space:]]\+TRACE' "$DEFINES" || true
else
    echo "install_jm.sh: WARNING — $DEFINES not found; trace output may be disabled" >&2
fi

# ---------------------------------------------------------------------
# Step 3: build.
# ---------------------------------------------------------------------
echo "install_jm.sh: building JM"
# Some JM distributions ship with CRLF line endings in shell scripts
# and Makefiles. Normalise before invoking make to avoid syntax errors
# on Linux toolchains.
find "$JM_DIR" -type f \( -name 'Makefile*' -o -name '*.sh' \) \
    -exec sed -i 's/\r$//' {} +

MAKE_JOBS="$(nproc 2>/dev/null || echo 2)"
if ! make -C "$JM_DIR" -j"$MAKE_JOBS" 2>&1 | tail -40; then
    echo "install_jm.sh: top-level make failed, trying ldecod only" >&2
    if ! make -C "$JM_DIR/ldecod" -j"$MAKE_JOBS" 2>&1 | tail -40; then
        echo "install_jm.sh: ldecod build failed" >&2
        echo "build-failed" > "$INSTALL_DIR/STATUS"
        exit 4
    fi
fi

# ---------------------------------------------------------------------
# Step 4: harvest binaries.
#
# JM's Makefile usually writes into `bin/` relative to its source root
# but older forks place it under `ldecod/bin/`. Accept either layout.
# ---------------------------------------------------------------------
copied=0
for cand in \
    "$JM_DIR/bin/ldecod.exe" \
    "$JM_DIR/ldecod/bin/ldecod.exe" \
    "$JM_DIR/bin/ldecod" \
    "$JM_DIR/ldecod/bin/ldecod" \
    ; do
    if [[ -f "$cand" ]]; then
        cp "$cand" "$INSTALL_DIR/bin/ldecod.exe"
        chmod +x "$INSTALL_DIR/bin/ldecod.exe"
        copied=1
        echo "install_jm.sh: installed $(basename "$cand") → $INSTALL_DIR/bin/ldecod.exe"
        break
    fi
done
if [[ "$copied" -eq 0 ]]; then
    echo "install_jm.sh: could not locate built ldecod binary" >&2
    find "$JM_DIR" -type f -name 'ldecod*' -executable 2>/dev/null | head -20 || true
    echo "harvest-failed" > "$INSTALL_DIR/STATUS"
    exit 5
fi

# lencod is a nice-to-have, not required for Phase D / E.
for cand in \
    "$JM_DIR/bin/lencod.exe" \
    "$JM_DIR/lencod/bin/lencod.exe" \
    "$JM_DIR/bin/lencod" \
    ; do
    if [[ -f "$cand" ]]; then
        cp "$cand" "$INSTALL_DIR/bin/lencod.exe"
        chmod +x "$INSTALL_DIR/bin/lencod.exe"
        break
    fi
done

# Record a sanity-check run of the decoder's --help so subsequent
# steps can trust that the binary is at least dynamically linkable.
"$INSTALL_DIR/bin/ldecod.exe" -h 2>&1 | head -5 > "$INSTALL_DIR/bin/ldecod.help" || true

echo "ready" > "$INSTALL_DIR/STATUS"
echo "install_jm.sh: done"
ls -la "$INSTALL_DIR/bin/"
