#!/usr/bin/env bash
set -euo pipefail

CMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}"

cmake -S . -B build -DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE"
cmake --build build --parallel
ctest --test-dir build --output-on-failure
