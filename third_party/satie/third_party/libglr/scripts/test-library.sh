#!/bin/bash
# test-library.sh - configure, build, and run the LibGLR test suite

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${PROJECT_DIR}/build"
BUILD_TYPE=Debug
VERBOSE=false
CLEAN=false
LABELS=""
SANITIZERS=false

usage() {
    cat <<USAGE
Usage: $0 [options]

Options:
  --clean           Remove the build directory before configuring
  --verbose         Run ctest with verbose output
  --label LABEL     Run only tests matching a CTest label
  --sanitizers      Configure tests with address/undefined sanitizers
  --build-type TYPE CMake build type (default: Debug)
  --help            Show this help message
USAGE
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --clean) CLEAN=true; shift ;;
        --verbose) VERBOSE=true; shift ;;
        --label) LABELS="$2"; shift 2 ;;
        --sanitizers) SANITIZERS=true; shift ;;
        --build-type) BUILD_TYPE="$2"; shift 2 ;;
        --help) usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage; exit 1 ;;
    esac
done

if [[ "$CLEAN" == true ]]; then
    rm -rf "$BUILD_DIR"
fi

cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DBUILD_TESTS=ON \
    -DBUILD_EXAMPLES=OFF \
    -DBUILD_DOCUMENTATION=OFF \
    -DENABLE_TEST_SANITIZERS=$([[ "$SANITIZERS" == true ]] && echo ON || echo OFF)

cmake --build "$BUILD_DIR"

ctest_args=(--test-dir "$BUILD_DIR" --output-on-failure)
if [[ "$VERBOSE" == true ]]; then
    ctest_args+=(--verbose)
fi
if [[ -n "$LABELS" ]]; then
    ctest_args+=(-L "$LABELS")
fi

ctest "${ctest_args[@]}"
