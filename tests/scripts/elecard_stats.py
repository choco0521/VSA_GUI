#!/usr/bin/env python3
"""Compute a human-readable NAL breakdown of an H.264 Annex B stream.

Used by the CI workflow to surface TheaterSquare numbers (NAL count,
SPS/PPS/IDR breakdown, sha256, file size) to the PR step summary and a
sticky comment so the figures are visible without digging through the
full Actions log.

Usage:
    elecard_stats.py <path>
"""

import hashlib
import os
import sys


def scan_nal_units(data: bytes) -> list[tuple[int, int, int]]:
    """Return a list of (payload_offset, payload_size, nal_type) tuples."""
    offsets: list[tuple[int, int]] = []  # (start_code_offset, start_code_len)
    i = 0
    n = len(data)
    while i < n:
        if i + 3 < n and data[i] == 0 and data[i + 1] == 0 and data[i + 2] == 0 and data[i + 3] == 1:
            offsets.append((i, 4))
            i += 4
        elif i + 2 < n and data[i] == 0 and data[i + 1] == 0 and data[i + 2] == 1:
            offsets.append((i, 3))
            i += 3
        else:
            i += 1

    spans: list[tuple[int, int, int]] = []
    for idx, (sc_off, sc_len) in enumerate(offsets):
        payload_off = sc_off + sc_len
        payload_end = offsets[idx + 1][0] if idx + 1 < len(offsets) else n
        payload_size = max(0, payload_end - payload_off)
        if payload_off >= n:
            spans.append((payload_off, payload_size, -1))
            continue
        nal_type = data[payload_off] & 0x1F
        spans.append((payload_off, payload_size, nal_type))
    return spans


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: elecard_stats.py <path>", file=sys.stderr)
        return 2

    path = sys.argv[1]
    if not os.path.isfile(path):
        print(f"{path}: file not found", file=sys.stderr)
        return 1

    with open(path, "rb") as f:
        data = f.read()

    size = len(data)
    sha256 = hashlib.sha256(data).hexdigest()
    spans = scan_nal_units(data)
    total_nal = len(spans)

    counts: dict[int, int] = {}
    for _off, _sz, t in spans:
        counts[t] = counts.get(t, 0) + 1

    type_labels = {
        1: "non-IDR slice",
        2: "data partition A",
        3: "data partition B",
        4: "data partition C",
        5: "IDR slice",
        6: "SEI",
        7: "SPS",
        8: "PPS",
        9: "AUD (access unit delimiter)",
        10: "end of sequence",
        11: "end of stream",
        12: "filler",
        13: "SPS extension",
        14: "prefix NAL",
        15: "subset SPS",
        19: "aux slice",
        20: "coded slice extension",
    }

    print(f"file:        {path}")
    print(f"size bytes:  {size:,}")
    print(f"sha256:      {sha256}")
    print(f"NAL units:   {total_nal}")
    print()
    print("  type  count  name")
    for t in sorted(counts.keys()):
        if t == -1:
            continue
        label = type_labels.get(t, "reserved / unknown")
        print(f"  {t:>4}  {counts[t]:>5}  {label}")

    # Also emit machine-readable KEY=VALUE lines for downstream scripting.
    print()
    print("--- machine readable ---")
    print(f"FILE_SIZE={size}")
    print(f"SHA256={sha256}")
    print(f"NAL_TOTAL={total_nal}")
    for t in sorted(counts.keys()):
        if t == -1:
            continue
        print(f"NAL_TYPE_{t}={counts[t]}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
