#!/bin/sh
# Build and run the native unit tests (needs cmake + a C compiler; no ESP-IDF).
set -e
cd "$(dirname "$0")/../test/host"
cmake -S . -B build -G "${CMAKE_GENERATOR:-Unix Makefiles}" >/dev/null
cmake --build build
cd build && ctest --output-on-failure "$@"
