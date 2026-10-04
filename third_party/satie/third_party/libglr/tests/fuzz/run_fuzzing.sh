#!/bin/bash

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$PROJECT_ROOT/build_fuzz}"

echo "=========================================="
echo "libglr AFL Fuzzing Setup"
echo "=========================================="
echo ""

if ! command -v afl-fuzz &> /dev/null; then
    echo "ERROR: AFL not found. Please install afl++:"
    echo "  Ubuntu/Debian: sudo apt-get install afl++"
    echo "  Fedora: sudo dnf install afl"
    echo "  macOS: brew install afl++"
    exit 1
fi

echo "Building with AFL instrumentation..."
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

export CC=afl-clang-fast
export CXX=afl-clang-fast++

if ! command -v afl-clang-fast &> /dev/null; then
    export CC=afl-gcc
    export CXX=afl-g++
fi

cmake "$PROJECT_ROOT" \
    -DENABLE_CACHE=ON \
    -DENABLE_FUZZING=ON \
    -DBUILD_TESTS=ON \
    -DCMAKE_BUILD_TYPE=Debug

cmake --build . -j$(nproc)

cd tests/fuzz

echo ""
echo "=========================================="
echo "Fuzzing targets built successfully!"
echo "=========================================="
echo ""
echo "Available targets:"
echo "  1. fuzz_cache          - Cache operations"
echo "  2. fuzz_serialization  - Serialization/deserialization"
echo "  3. fuzz_diff           - Diff computation"
echo ""
echo "To run fuzzing (example):"
echo "  afl-fuzz -i corpus_cache -o findings_cache ./fuzz_cache"
echo ""
echo "To run in parallel (4 instances):"
echo "  afl-fuzz -i corpus_cache -o findings_cache -M fuzzer01 ./fuzz_cache &"
echo "  afl-fuzz -i corpus_cache -o findings_cache -S fuzzer02 ./fuzz_cache &"
echo "  afl-fuzz -i corpus_cache -o findings_cache -S fuzzer03 ./fuzz_cache &"
echo "  afl-fuzz -i corpus_cache -o findings_cache -S fuzzer04 ./fuzz_cache &"
echo ""
echo "Monitor with: afl-whatsup findings_cache"
echo ""

read -p "Start fuzzing cache target now? (y/N) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo "Starting AFL fuzzer for cache target..."
    echo "Press Ctrl+C to stop"
    sleep 2
    afl-fuzz -i corpus_cache -o findings_cache ./fuzz_cache
fi
