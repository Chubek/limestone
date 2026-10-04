#!/bin/bash

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${BUILD_DIR:-$PROJECT_ROOT/build}"

echo "=========================================="
echo "libglr Comprehensive Test Suite"
echo "=========================================="
echo ""

if [ ! -d "$BUILD_DIR" ]; then
    echo "Build directory not found. Building project..."
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    cmake .. -DENABLE_CACHE=ON -DBUILD_TESTS=ON
    cmake --build . -j$(nproc)
else
    echo "Using existing build directory: $BUILD_DIR"
fi

cd "$BUILD_DIR"

echo ""
echo "=========================================="
echo "1. Running Original Test Suite"
echo "=========================================="
ctest --output-on-failure -L "unit;core" || true

echo ""
echo "=========================================="
echo "2. Running Catch2 Tests"
echo "=========================================="
if [ -f "tests/catch2/catch2_test_cache" ]; then
    echo "Running cache tests..."
    ./tests/catch2/catch2_test_cache || true
    
    echo "Running diff tests..."
    ./tests/catch2/catch2_test_diff || true
    
    echo "Running serialization tests..."
    ./tests/catch2/catch2_test_serialization || true
    
    echo "Running incremental parsing tests..."
    ./tests/catch2/catch2_test_incremental || true
else
    echo "Catch2 tests not built. Skipping."
fi

echo ""
echo "=========================================="
echo "3. Test Summary"
echo "=========================================="
ctest -N

echo ""
echo "=========================================="
echo "All tests completed!"
echo "=========================================="
