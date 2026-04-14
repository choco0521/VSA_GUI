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
#     <install_dir>/bin/lencod.exe     (optional)
#     <install_dir>/VERSION            (git rev or tarball filename)
#     <install_dir>/STATUS             (ready | fetch-failed | build-failed
#                                       | harvest-failed | layout-error)
#     <install_dir>/DIAGNOSTIC.txt     (every informational line emitted
#                                       by the script, so a single
#                                       `cat` from the CI status step
#                                       surfaces the full build log)
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
# Diagnostic log. Everything informational the script produces is also
# appended to $INSTALL_DIR/DIAGNOSTIC.txt so the "Show JM install
# status" step can surface one single file to the PR reviewer instead
# of requiring a click-through to each upstream build step.
# ---------------------------------------------------------------------
DIAG="$INSTALL_DIR/DIAGNOSTIC.txt"
: > "$DIAG"
diag() {
    printf '%s\n' "$*" | tee -a "$DIAG"
}
diag_hr() {
    diag ""
    diag "---------- $* ----------"
}
diag_tail() {
    local n="$1" path="$2"
    [[ -f "$path" ]] || return 0
    diag "(last $n lines of $path)"
    tail -n "$n" "$path" 2>/dev/null | sed 's/^/    /' | tee -a "$DIAG" >/dev/null
}

diag "install_jm.sh starting at $(date -u +%Y-%m-%dT%H:%M:%SZ)"
diag "INSTALL_DIR=$INSTALL_DIR"
diag "WORK_DIR=$WORK_DIR"
diag "uname:  $(uname -a)"
diag "gcc:    $(gcc --version 2>&1 | head -1 || true)"
diag "make:   $(make --version 2>&1 | head -1 || true)"
diag "cmake:  $(cmake --version 2>&1 | head -1 || true)"
diag "bash:   $BASH_VERSION"

# ---------------------------------------------------------------------
# Step 1: fetch the source tree.
# ---------------------------------------------------------------------
diag_hr "Step 1: fetch JM source"
cd "$WORK_DIR"
JM_DIR=""

try_git() {
    local url="$1"
    diag "git clone --depth 1 $url"
    if git clone --depth 1 "$url" JM >>"$DIAG" 2>&1; then
        JM_DIR="$WORK_DIR/JM"
        (cd JM && git rev-parse HEAD > "$INSTALL_DIR/VERSION") || true
        diag "  → clone OK, HEAD=$(cat "$INSTALL_DIR/VERSION" 2>/dev/null || echo unknown)"
        return 0
    fi
    diag "  → clone FAILED"
    return 1
}

try_tarball() {
    local url="$1" name="$2"
    diag "curl -fsSL $url"
    if curl -fsSL --retry 3 --retry-delay 2 "$url" -o "$name" >>"$DIAG" 2>&1; then
        diag "  → download OK"
        unzip -q "$name" 2>>"$DIAG" || tar xf "$name" 2>>"$DIAG" || {
            diag "  → archive extract FAILED"
            return 1
        }
        if [[ -d JM ]]; then
            JM_DIR="$WORK_DIR/JM"
        else
            JM_DIR="$(find . -maxdepth 2 -type d -name 'JM*' | head -n 1 || true)"
        fi
        [[ -n "$JM_DIR" && -d "$JM_DIR" ]] || return 1
        basename "$url" > "$INSTALL_DIR/VERSION"
        diag "  → extract OK, JM_DIR=$JM_DIR"
        return 0
    fi
    diag "  → download FAILED"
    return 1
}

if ! try_git https://vcgit.hhi.fraunhofer.de/jvet/JM.git; then
    if ! try_git https://github.com/OpenHEVC/JM.git; then
        if ! try_tarball http://iphome.hhi.de/suehring/tml/download/jm19.0.zip jm19.0.zip; then
            diag "FATAL: all JM fetch paths failed"
            echo "fetch-failed" > "$INSTALL_DIR/STATUS"
            exit 2
        fi
    fi
fi

if [[ -z "$JM_DIR" || ! -d "$JM_DIR" ]]; then
    diag "FATAL: unpacked JM directory not found"
    echo "layout-error" > "$INSTALL_DIR/STATUS"
    exit 3
fi

# ---------------------------------------------------------------------
# Step 2: inspect layout — critical for understanding what build
# system JM is shipping on this particular commit.
# ---------------------------------------------------------------------
diag_hr "Step 2: JM directory layout"
diag "JM_DIR=$JM_DIR"
diag "top-level entries:"
ls -la "$JM_DIR" | tee -a "$DIAG" >/dev/null

diag ""
diag "top two levels (directories only):"
find "$JM_DIR" -maxdepth 2 -type d | sort | tee -a "$DIAG" >/dev/null

diag ""
diag "build-system files in the top two levels:"
find "$JM_DIR" -maxdepth 2 -type f \
     \( -name 'Makefile*' -o -name 'CMakeLists.txt' \
        -o -name 'configure*' -o -name '*.pro' -o -name 'meson.build' \) \
     2>/dev/null | tee -a "$DIAG" >/dev/null

# ---------------------------------------------------------------------
# Step 3: enable TRACE output on the decoder.
# ---------------------------------------------------------------------
diag_hr "Step 3: patch TRACE macro"
DEFINES="$JM_DIR/ldecod/inc/defines.h"
if [[ -f "$DEFINES" ]]; then
    diag "patching $DEFINES"
    sed -i.bak -E 's@^(#define[[:space:]]+TRACE[[:space:]]+)[0-9]+.*$@\11@' "$DEFINES"
    diag "  after patch:"
    grep -n '#define[[:space:]]\+TRACE' "$DEFINES" 2>/dev/null \
        | sed 's/^/    /' | tee -a "$DIAG" >/dev/null || true
else
    diag "WARNING: $DEFINES not found — trace output may stay disabled"
    diag "searching for a substitute defines.h:"
    find "$JM_DIR" -name defines.h 2>/dev/null | tee -a "$DIAG" >/dev/null
fi

# Some JM distributions ship Windows CRLF line endings in shell
# scripts and Makefiles. Normalise them before invoking make.
find "$JM_DIR" -type f \( -name 'Makefile*' -o -name '*.sh' \) \
    -exec sed -i 's/\r$//' {} + 2>>"$DIAG" || true

# ---------------------------------------------------------------------
# Step 4: build. Try Makefile first (classic JM), fall back to a
# CMake out-of-source build if the tree only ships with
# CMakeLists.txt.
# ---------------------------------------------------------------------
diag_hr "Step 4: build"
MAKE_JOBS="$(nproc 2>/dev/null || echo 2)"
diag "using $MAKE_JOBS parallel jobs"

run_in_diag() {
    # Run a command, stream its stdout+stderr into the diagnostic log
    # with a 4-space indent, and forward the exit code of the command
    # itself (not tee's) back to the caller.
    ( "$@" ) 2>&1 | sed 's/^/    /' | tee -a "$DIAG" >/dev/null
    return "${PIPESTATUS[0]}"
}

build_ok=0
make_rc=255
cmake_rc=255

if [[ -f "$JM_DIR/Makefile" ]]; then
    diag "trying top-level make"
    if run_in_diag make -C "$JM_DIR" -j"$MAKE_JOBS"; then
        build_ok=1
        make_rc=0
    else
        make_rc=$?
        diag "top-level make exit=$make_rc, falling back to per-subdir"
        if [[ -d "$JM_DIR/lcommon" ]] && run_in_diag make -C "$JM_DIR/lcommon" -j"$MAKE_JOBS"; then
            : # common library built
        fi
        if [[ -d "$JM_DIR/ldecod" ]] && run_in_diag make -C "$JM_DIR/ldecod" -j"$MAKE_JOBS"; then
            build_ok=1
        fi
    fi
fi

if [[ "$build_ok" -ne 1 && -f "$JM_DIR/CMakeLists.txt" ]]; then
    diag "trying cmake out-of-source build"
    mkdir -p "$JM_DIR/build"
    if run_in_diag cmake -S "$JM_DIR" -B "$JM_DIR/build" \
           -DCMAKE_BUILD_TYPE=Release \
           && run_in_diag cmake --build "$JM_DIR/build" -j "$MAKE_JOBS"; then
        build_ok=1
        cmake_rc=0
    else
        cmake_rc=$?
        diag "cmake path exit=$cmake_rc"
    fi
fi

if [[ "$build_ok" -ne 1 ]]; then
    diag "FATAL: no build system produced a working artifact"
    diag "make_rc=$make_rc cmake_rc=$cmake_rc"
    echo "build-failed" > "$INSTALL_DIR/STATUS"
    exit 4
fi

# ---------------------------------------------------------------------
# Step 5: harvest the built binaries.
#
# Locate any executable file whose basename matches ldecod / lencod
# anywhere under $JM_DIR and copy the first ELF match into the
# install directory. A static list of expected paths has bit us
# twice now (different JM forks use different output locations), so
# this version of the script is deliberately structure-agnostic.
# ---------------------------------------------------------------------
diag_hr "Step 5: harvest binaries"

diag "directories containing executables under $JM_DIR:"
find "$JM_DIR" -type f -executable -printf '%h\n' 2>/dev/null \
    | sort -u | tee -a "$DIAG" >/dev/null

diag ""
diag "all files whose basename starts with ldecod:"
find "$JM_DIR" -type f -iname 'ldecod*' 2>/dev/null \
    | tee -a "$DIAG" >/dev/null || true

diag ""
diag "all files whose basename starts with lencod:"
find "$JM_DIR" -type f -iname 'lencod*' 2>/dev/null \
    | tee -a "$DIAG" >/dev/null || true

diag ""
diag "every ELF / Mach-O object under $JM_DIR (paths only, first 40):"
if command -v file >/dev/null 2>&1; then
    find "$JM_DIR" -type f -exec file {} + 2>/dev/null \
        | grep -E 'ELF.*executable|Mach-O.*executable' \
        | head -40 | tee -a "$DIAG" >/dev/null || true
fi

pick_binary() {
    # Given a stem (ldecod or lencod), return the path to the first
    # ELF/Mach-O executable whose basename begins with that stem.
    local stem="$1"
    local p
    while IFS= read -r p; do
        [[ -f "$p" ]] || continue
        case "$p" in
            *.c|*.h|*.o|*.bak|*~) continue ;;
        esac
        if command -v file >/dev/null 2>&1; then
            if file "$p" 2>/dev/null | grep -qE 'ELF.*executable|Mach-O.*executable'; then
                printf '%s\n' "$p"
                return 0
            fi
        else
            local sz
            sz="$(stat -c '%s' "$p" 2>/dev/null || echo 0)"
            if [[ -x "$p" && "$sz" -ge 10240 ]]; then
                printf '%s\n' "$p"
                return 0
            fi
        fi
    done < <(find "$JM_DIR" -type f -iname "${stem}*" 2>/dev/null)
    return 1
}

ldecod_path="$(pick_binary ldecod || true)"
if [[ -z "$ldecod_path" ]]; then
    diag "FATAL: no ldecod executable found under $JM_DIR"
    diag "full file inventory (first 120 entries):"
    find "$JM_DIR" -type f 2>/dev/null | head -120 \
        | tee -a "$DIAG" >/dev/null
    echo "harvest-failed" > "$INSTALL_DIR/STATUS"
    exit 5
fi

cp "$ldecod_path" "$INSTALL_DIR/bin/ldecod.exe"
chmod +x "$INSTALL_DIR/bin/ldecod.exe"
diag "installed ldecod: $ldecod_path → $INSTALL_DIR/bin/ldecod.exe"

lencod_path="$(pick_binary lencod || true)"
if [[ -n "$lencod_path" ]]; then
    cp "$lencod_path" "$INSTALL_DIR/bin/lencod.exe"
    chmod +x "$INSTALL_DIR/bin/lencod.exe"
    diag "installed lencod: $lencod_path → $INSTALL_DIR/bin/lencod.exe"
else
    diag "lencod not found (optional, not fatal)"
fi

# ---------------------------------------------------------------------
# Step 6: sanity-run ldecod so the binary is known-good before the
# trace-diff step depends on it.
# ---------------------------------------------------------------------
diag_hr "Step 6: sanity ldecod --help"
if "$INSTALL_DIR/bin/ldecod.exe" -h >"$INSTALL_DIR/bin/ldecod.help" 2>&1; then
    diag "ldecod -h exit=0"
else
    diag "ldecod -h exit=$? (non-zero is acceptable — many JM builds exit 2 after dumping usage)"
fi
diag_tail 10 "$INSTALL_DIR/bin/ldecod.help"

echo "ready" > "$INSTALL_DIR/STATUS"
diag ""
diag "install_jm.sh: done, STATUS=ready"
ls -la "$INSTALL_DIR/bin/" | tee -a "$DIAG" >/dev/null
