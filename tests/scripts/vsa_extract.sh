#!/usr/bin/env bash
#
# vsa_extract — detect the container and codec of an arbitrary video
# file, then invoke ffmpeg to produce the raw elementary stream that
# vsa_parse / vsa_codec can consume directly. This is the runtime
# entry point for Phase G of the VSA_GUI roadmap: the GUI and headless
# diagnostic tooling never open containers themselves, they always go
# through this script (or a future C++ port of it) so we avoid
# linking libavformat and stay LGPL-free.
#
# Usage:
#     vsa_extract <input> [output]
#
# If the output path is omitted, writes to `$(mktemp --suffix=.<ext>)`
# and prints that path on stdout. All informational output goes to
# stderr so the stdout path is pipe-friendly:
#
#     raw=$(vsa_extract movie.mp4)
#     vsa_parse "$raw" --json
#
# Supported codec → elementary stream mapping (driven by ffprobe's
# codec_name classification):
#
#     h264, avc          → .h264 raw Annex B (with h264_mp4toannexb)
#     hevc, h265         → .h265 raw Annex B (with hevc_mp4toannexb)
#     vvc, h266          → .h266 raw Annex B (with vvc_mp4toannexb)
#     av1                → .ivf
#     vp9                → .ivf
#     mpeg2video, mpeg1  → .m2v elementary stream
#     avs3               → .avs3 (ffmpeg support varies)
#
# Container detection is automatic: MP4, MOV, MKV, WebM, TS, PS, AVI,
# FLV, MXF, HEIC and IVF are all transparent to the caller. The
# relevant ffprobe format_name is echoed to stderr for visibility.

set -euo pipefail

usage() {
    cat >&2 <<EOF
usage: vsa_extract <input> [output]

Detect the container and codec of <input> and produce a raw
elementary stream suitable for vsa_parse. When [output] is omitted,
a mkstemp path is allocated and printed on stdout.
EOF
    exit 2
}

[[ $# -ge 1 && $# -le 2 ]] || usage

IN="$1"
OUT="${2:-}"

if [[ ! -f "$IN" ]]; then
    echo "vsa_extract: $IN: no such file" >&2
    exit 3
fi

command -v ffprobe >/dev/null 2>&1 || {
    echo "vsa_extract: ffprobe not found on PATH" >&2
    exit 4
}
command -v ffmpeg >/dev/null 2>&1 || {
    echo "vsa_extract: ffmpeg not found on PATH" >&2
    exit 4
}

# ---------------------------------------------------------------------
# Probe. We ask for JSON so parsing is stable across ffprobe versions.
# ---------------------------------------------------------------------
PROBE_JSON="$(ffprobe -v error -of json -show_format \
                -show_streams -select_streams v:0 "$IN")"

# format_name and codec_name both come out as "foo,bar" strings, so
# awk/sed is easier than a full JSON parser here.
FORMAT_NAME="$(printf '%s\n' "$PROBE_JSON" \
    | sed -n 's/.*"format_name"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' | head -1)"
CODEC_NAME="$(printf '%s\n' "$PROBE_JSON" \
    | sed -n 's/.*"codec_name"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' | head -1)"
WIDTH="$(printf '%s\n'  "$PROBE_JSON" \
    | sed -n 's/.*"width"[[:space:]]*:[[:space:]]*\([0-9]*\).*/\1/p' | head -1)"
HEIGHT="$(printf '%s\n' "$PROBE_JSON" \
    | sed -n 's/.*"height"[[:space:]]*:[[:space:]]*\([0-9]*\).*/\1/p' | head -1)"

if [[ -z "$CODEC_NAME" ]]; then
    echo "vsa_extract: could not determine video codec in $IN" >&2
    echo "vsa_extract: ffprobe output was:" >&2
    printf '%s\n' "$PROBE_JSON" | sed 's/^/  /' >&2
    exit 5
fi

echo "vsa_extract: container=$FORMAT_NAME codec=$CODEC_NAME ${WIDTH}x${HEIGHT}" >&2

# ---------------------------------------------------------------------
# Codec → extraction recipe. $BSF may be empty if no bitstream filter
# is needed (IVF / raw MPEG-2).
# ---------------------------------------------------------------------
EXT=""
FFMT=""
BSF=()
case "$CODEC_NAME" in
    h264|avc1)
        EXT="h264";   FFMT="h264"
        BSF=(-bsf:v h264_mp4toannexb)
        ;;
    hevc|h265|hvc1)
        EXT="h265";   FFMT="hevc"
        BSF=(-bsf:v hevc_mp4toannexb)
        ;;
    vvc|h266|vvc1)
        EXT="h266";   FFMT="hevc"  # ffmpeg labels "hevc" for h266 output container too in some builds
        BSF=(-bsf:v vvc_mp4toannexb)
        ;;
    av1)
        EXT="ivf";    FFMT="ivf";   BSF=()
        ;;
    vp9)
        EXT="ivf";    FFMT="ivf";   BSF=()
        ;;
    mpeg2video|mpeg1video)
        EXT="m2v";    FFMT="mpeg2video"; BSF=()
        ;;
    avs3)
        EXT="avs3";   FFMT="avs3";  BSF=()
        ;;
    *)
        echo "vsa_extract: unsupported codec '$CODEC_NAME' — no extraction recipe" >&2
        exit 6
        ;;
esac

if [[ -z "$OUT" ]]; then
    OUT="$(mktemp --suffix=".$EXT")"
fi

# -nostdin prevents ffmpeg from swallowing the caller's stdin when
# the script is used from a pipe. -map 0:v:0 restricts copying to
# the first video stream so multi-track containers don't confuse
# downstream parsers.
if ! ffmpeg -v error -nostdin -y \
        -i "$IN" \
        -map 0:v:0 \
        -c:v copy \
        "${BSF[@]}" \
        -f "$FFMT" \
        "$OUT" \
        < /dev/null; then
    echo "vsa_extract: ffmpeg extraction failed" >&2
    exit 7
fi

if [[ ! -s "$OUT" ]]; then
    echo "vsa_extract: ffmpeg produced an empty file at $OUT" >&2
    exit 7
fi

echo "vsa_extract: wrote $(stat -c '%s' "$OUT") bytes to $OUT" >&2
printf '%s\n' "$OUT"
