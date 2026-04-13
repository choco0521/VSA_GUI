#!/usr/bin/env bash
#
# install_jm.sh — download and build the JM H.264 reference software
# with TRACE output enabled. Invoked from .github/workflows/ci.yml only
# on cache misses.
#
# Usage: install_jm.sh <install_dir>
#
# Phase A: this is a stub that just creates the target directory and
# writes a sentinel file. Phase D replaces the body with the real
# download / patch / make steps and adds the ldecod.exe invocation.
#
# Reference for the real implementation:
#   curl -L -o jm.zip http://iphome.hhi.de/suehring/tml/download/jm19.0.zip
#   unzip jm.zip
#   cd JM
#   # Ensure TRACE is enabled in ldecod
#   sed -i 's/^#define TRACE .*$/#define TRACE 1/' ldecod/inc/defines.h
#   make -C ldecod
#   cp bin/ldecod.exe "$1/"

set -euo pipefail

install_dir="${1:-$HOME/.cache/jm}"
mkdir -p "$install_dir"
echo "phase-a-stub" > "$install_dir/STATUS"
echo "install_jm.sh: wrote stub sentinel to $install_dir/STATUS"
