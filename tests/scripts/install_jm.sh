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

echo "install_jm.sh: initial JM directory layout (top two levels):"
find "$JM_DIR" -maxdepth 2 -type d | sort
echo "install_jm.sh: top-level Makefile target listing:"
grep -E '^[A-Za-z0-9_.-]+:' "$JM_DIR/Makefile" 2>/dev/null | head -20 || true

MAKE_JOBS="$(nproc 2>/dev/null || echo 2)"

# Pipe `make` output through tee so we can both tail it to the log
# and get the real exit status via ${PIPESTATUS[0]}. A bare `make | tail`
# would hide build failures behind tail's exit code 0 and let the
# harvest step run on an incomplete tree.
make_with_status() {
    local target_dir="$1"
    (cd "$target_dir" && make -j"$MAKE_JOBS" 2>&1) \
        | tee "$WORK_DIR/make.log" | tail -60
    return "${PIPESTATUS[0]}"
}

build_ok=0
if make_with_status "$JM_DIR"; then
    build_ok=1
else
    echo "install_jm.sh: top-level make failed (exit $?), trying lcommon + ldecod" >&2
    if make_with_status "$JM_DIR/lcommon" \
       && make_with_status "$JM_DIR/ldecod"; then
        build_ok=1
    fi
fi

if [[ "$build_ok" -ne 1 ]]; then
    echo "install_jm.sh: build failed, last 80 lines of make.log:" >&2
    tail -80 "$WORK_DIR/make.log" >&2 || true
    echo "build-failed" > "$INSTALL_DIR/STATUS"
    exit 4
fi

# ---------------------------------------------------------------------
# Step 4: harvest binaries.
#
# JM upstream versions and community forks all place the built
# decoder in different locations. Rather than enumerate every
# historical spelling, locate *any* executable file whose basename
# starts with `ldecod` anywhere under $JM_DIR and take the first
# match. The `file` command is preferred to distinguish the built
# ELF from a leftover source file of the same stem, with a
# size-based fallback when `file` is not installed on the runner.
# ---------------------------------------------------------------------
echo "install_jm.sh: searching for built ldecod binary"
mapfile -t LDECOD_CANDIDATES < <(
    find "$JM_DIR" -type f \
         \( -name 'ldecod.exe' -o -name 'ldecod' \) \
         -not -name '*.c' -not -name '*.h' 2>/dev/null
)
printf 'install_jm.sh: candidate: %s\n' "${LDECOD_CANDIDATES[@]}"

chosen=""
for cand in "${LDECOD_CANDIDATES[@]}"; do
    if command -v file >/dev/null 2>&1; then
        if file "$cand" 2>/dev/null | grep -qE 'ELF.*executable|Mach-O.*executable'; then
            chosen="$cand"
            break
        fi
    else
        # No `file` available: fall back to "executable bit set AND
        # at least 10 kB" which is comfortably above any shell stub.
        sz="$(stat -c '%s' "$cand" 2>/dev/null || echo 0)"
        if [[ -x "$cand" && "$sz" -ge 10240 ]]; then
            chosen="$cand"
            break
        fi
    fi
done

if [[ -z "$chosen" ]]; then
    echo "install_jm.sh: could not locate built ldecod binary" >&2
    echo "install_jm.sh: full JM tree after build (first 80 entries):" >&2
    find "$JM_DIR" -type f | head -80 >&2
    echo "install_jm.sh: all ELF files found under $JM_DIR:" >&2
    find "$JM_DIR" -type f -exec file {} + 2>/dev/null \
        | grep -E 'ELF|Mach-O' | head -40 >&2 || true
    echo "harvest-failed" > "$INSTALL_DIR/STATUS"
    exit 5
fi

cp "$chosen" "$INSTALL_DIR/bin/ldecod.exe"
chmod +x "$INSTALL_DIR/bin/ldecod.exe"
echo "install_jm.sh: installed $chosen → $INSTALL_DIR/bin/ldecod.exe"

# lencod is a nice-to-have, not required for Phase D / E.
mapfile -t LENCOD_CANDIDATES < <(
    find "$JM_DIR" -type f \
         \( -name 'lencod.exe' -o -name 'lencod' \) \
         -not -name '*.c' -not -name '*.h' 2>/dev/null
)
for cand in "${LENCOD_CANDIDATES[@]}"; do
    if [[ -x "$cand" ]] && \
       (command -v file >/dev/null 2>&1 \
        && file "$cand" 2>/dev/null | grep -qE 'ELF.*executable'); then
        cp "$cand" "$INSTALL_DIR/bin/lencod.exe"
        chmod +x "$INSTALL_DIR/bin/lencod.exe"
        echo "install_jm.sh: installed $cand → $INSTALL_DIR/bin/lencod.exe"
        break
    fi
done

# Record a sanity-check run of the decoder's --help so subsequent
# steps can trust that the binary is at least dynamically linkable.
"$INSTALL_DIR/bin/ldecod.exe" -h 2>&1 | head -5 > "$INSTALL_DIR/bin/ldecod.help" || true

echo "ready" > "$INSTALL_DIR/STATUS"
echo "install_jm.sh: done"
ls -la "$INSTALL_DIR/bin/"
