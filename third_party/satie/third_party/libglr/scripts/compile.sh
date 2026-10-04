#!/bin/bash
# compile.sh - Build LibGLR library
#
# Usage: ./scripts/compile.sh [options]
# Options:
#   --debug      Build in debug mode
#   --release    Build in release mode (default)
#   --clean      Clean before building
#   --help       Show help message

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${PROJECT_DIR}/build"

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

log_info() { echo -e "${GREEN}[INFO]${NC} $1"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

usage() {
    cat << USAGE
Usage: $(basename "$0") [options]

Build LibGLR library.

Options:
  --debug      Build in debug mode (-g -O0)
  --release    Build in release mode (-O3, default)
  --clean      Clean build directory before building
  --verbose    Show verbose output
  --help       Show this help message

USAGE
}

BUILD_TYPE="Release"
CLEAN=false
VERBOSE=""

while [[ $# -gt 0 ]]; do
    case $1 in
        --debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        --release)
            BUILD_TYPE="Release"
            shift
            ;;
        --clean)
            CLEAN=true
            shift
            ;;
        --verbose)
            VERBOSE="-v"
            shift
            ;;
        --help)
            usage
            exit 0
            ;;
        *)
            log_error "Unknown option: $1"
            usage
            exit 1
            ;;
    esac
done

log_info "Building LibGLR (${BUILD_TYPE})..."

if [[ "$CLEAN" == "true" ]]; then
    log_info "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake .. -DCMAKE_BUILD_TYPE="$BUILD_TYPE" $VERBOSE
cmake --build . $VERBOSE

log_info "Build complete: ${BUILD_DIR}/libglr.a"
