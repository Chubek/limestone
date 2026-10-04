#!/bin/bash
# install.sh - Install LibGLR library
#
# Usage: ./scripts/install.sh [options]
# Options:
#   --prefix=DIR    Installation prefix (default: /usr/local)
#   --build         Build before installing
#   --clean         Clean build before building
#   --help          Show help message

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

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

Install LibGLR library to system.

Options:
  --prefix=DIR    Installation prefix (default: /usr/local)
  --build         Build before installing
  --clean         Clean build before building
  --user          Install to user local directory (~/.local)
  --help          Show this help message

USAGE
}

PREFIX="/usr/local"
BUILD=false
CLEAN=false
USER_INSTALL=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --prefix=*)
            PREFIX="${1#*=}"
            shift
            ;;
        --prefix)
            PREFIX="$2"
            shift 2
            ;;
        --build)
            BUILD=true
            shift
            ;;
        --clean)
            CLEAN=true
            shift
            ;;
        --user)
            USER_INSTALL=true
            PREFIX="$HOME/.local"
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

if [[ "$BUILD" == "true" ]]; then
    log_info "Building LibGLR..."
    "$SCRIPT_DIR/compile.sh" --release --${CLEAN:+clean}
fi

log_info "Installing LibGLR to ${PREFIX}..."

cd "${PROJECT_DIR}/build"

if [[ "$USER_INSTALL" == "true" ]]; then
    sudo make install DESTDIR="$HOME"
else
    sudo make install
fi

log_info "Installation complete"
