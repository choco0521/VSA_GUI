#!/usr/bin/env bash
#
# fetch_elecard_sample.sh — download the Elecard TheaterSquare H.264
# sample and convert it to a raw Annex B elementary stream so the JM
# reference decoder and our vsa_annexb_find_nals scanner can consume
# it directly.
#
# Usage:
#     fetch_elecard_sample.sh [dest_dir]
#
# Produces:
#     <dest_dir>/theater_square.h264       (raw Annex B stream)
#     <dest_dir>/theater_square.mp4        (intermediate MP4 download)
#     <dest_dir>/theater_square.sha256     (recorded sha256 for pinning)
#
# If the final .h264 already exists the script is a no-op, which lets
# the GitHub Actions cache short-circuit the network call entirely.
#
# This file is not committed to the repository as a binary; only this
# script and its recorded SHA256 metadata live in the tree.

set -uo pipefail

DEST_DIR="${1:-/tmp/vsa-samples}"
URL="https://www.elecard.com/storage/video/TheaterSquare_1920x1080.mp4"
MP4_PATH="${DEST_DIR}/theater_square.mp4"
H264_PATH="${DEST_DIR}/theater_square.h264"
SHA_PATH="${DEST_DIR}/theater_square.sha256"

mkdir -p "${DEST_DIR}"

if [[ -s "${H264_PATH}" ]]; then
    echo "fetch_elecard_sample.sh: ${H264_PATH} already present, skipping"
    ls -la "${H264_PATH}"
    exit 0
fi

echo "fetch_elecard_sample.sh: downloading ${URL}"
if ! curl -fsSL --retry 3 --retry-delay 2 \
        -A "Mozilla/5.0 (vsa_gui-ci bot)" \
        -o "${MP4_PATH}" "${URL}"; then
    echo "fetch_elecard_sample.sh: curl failed" >&2
    exit 1
fi

echo "fetch_elecard_sample.sh: MP4 size: $(stat -c '%s' "${MP4_PATH}") bytes"
sha256sum "${MP4_PATH}" | tee "${SHA_PATH}"

echo "fetch_elecard_sample.sh: extracting Annex B elementary stream"
if ! ffmpeg -y -v error -i "${MP4_PATH}" \
        -c:v copy -bsf:v h264_mp4toannexb \
        -f h264 "${H264_PATH}"; then
    echo "fetch_elecard_sample.sh: ffmpeg extraction failed" >&2
    exit 1
fi

echo "fetch_elecard_sample.sh: H264 size: $(stat -c '%s' "${H264_PATH}") bytes"
ls -la "${H264_PATH}"
