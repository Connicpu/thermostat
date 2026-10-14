#!/usr/bin/env bash
# Build the host simulator and run every HOOTL scenario. Extra args go to hootl.py
# (e.g. -k sht40 --keep /tmp/hootl --junit hootl.xml).
# Needs managed_components/ (esp-zigbee-lib headers): run `idf.py reconfigure` once.
set -euo pipefail
cd "$(dirname "$0")/.."
cmake -S test/hootl -B test/hootl/build -G "${CMAKE_GENERATOR:-Ninja}" >/dev/null
cmake --build test/hootl/build
exec python3 test/hootl/hootl.py -j "$(nproc)" "$@"
