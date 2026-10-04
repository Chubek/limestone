#!/bin/bash
# build-docs.sh - Build LibGLR documentation
#
# Usage: ./scripts/build-docs.sh [options]
# Options:
#   --doxygen   Build Doxygen documentation
#   --man       Build manpage
#   --all       Build all documentation (default)
#   --help      Show help message

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${PROJECT_DIR}/build"

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

Build LibGLR documentation.

Options:
  --doxygen   Build Doxygen API documentation
  --man       Install manpage
  --all       Build all documentation (default)
  --help      Show this help message

USAGE
}

BUILD_DOXYGEN=false
BUILD_MAN=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --doxygen)
            BUILD_DOXYGEN=true
            shift
            ;;
        --man)
            BUILD_MAN=true
            shift
            ;;
        --all)
            BUILD_DOXYGEN=true
            BUILD_MAN=true
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

# Default to building all if nothing specified
if [[ "$BUILD_DOXYGEN" == "false" && "$BUILD_MAN" == "false" ]]; then
    BUILD_DOXYGEN=true
    BUILD_MAN=true
fi

cd "$PROJECT_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

if [[ "$BUILD_DOXYGEN" == "true" ]]; then
    log_info "Building Doxygen documentation..."
    
    if command -v doxygen &> /dev/null; then
        doxygen Doxyfile 2>/dev/null || {
            log_warn "Doxygen not found or failed"
        }
    else
        log_warn "Doxygen not installed, skipping Doxygen docs"
    fi
fi

if [[ "$BUILD_MAN" == "true" ]]; then
    log_info "Installing manpage..."
    
    if [[ -f "${PROJECT_DIR}/man/libglr.3tb" ]]; then
        if id -n www-data &>/dev/null 2>&1; then
            sudo install -m 644 "${PROJECT_DIR}/man/libglr.3tb" /usr/share/man/man3/
            log_info "Manpage installed to /usr/share/man/man3/"
        else
            install -m 644 "${PROJECT_DIR}/man/libglr.3tb" /usr/share/man/man3/ 2>/dev/null || \
                install -m 644 "${PROJECT_DIR}/man/libglr.3tb" /usr/local/share/man/man3/ || \
                log_warn "Could not install manpage (permission denied)"
        fi
    else
        log_warn "Manpage not found at ${PROJECT_DIR}/man/libglr.3tb"
    fi
fi

log_info "Documentation build complete"
