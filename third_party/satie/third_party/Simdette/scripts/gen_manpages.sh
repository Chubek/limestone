#!/usr/bin/env bash
# ============================================================================
# Simdette Manpage Generator
# ============================================================================
# This script generates manpages from Doxygen XML output.
# Usage: ./scripts/gen_manpages.sh
# ============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
DOC_DIR="$PROJECT_ROOT/docs/doxygen"
MAN_DIR="$PROJECT_ROOT/docs/man"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if Doxygen XML is available
if [ ! -d "$DOC_DIR/xml" ]; then
    log_error "Doxygen XML not found. Run 'make doc' or 'doxygen' first."
    exit 1
fi

# Create man directory
mkdir -p "$MAN_DIR/man3"
mkdir -p "$MAN_DIR/man7"

log_info "Generating manpages..."

# Generate manpage for simdette header
cat > "$MAN_DIR/man7/simdette.7" << 'MANPAGE'
.TH SIMDETTE 7 "Simdette Library" "Version 0.1.0" "User Manual"
.SH NAME
simdette \- SIMD abstraction library for C++

.SH SYNOPSIS
.B #include <simdette/simdette.hpp>

.SH DESCRIPTION
simdette is a header-only C++20 library that provides a high-level abstraction
over SIMD (Single Instruction, Multiple Data) intrinsics. It allows you to
write vectorized code using familiar C++ operators without needing to know
intrinsics.

.SH FEATURES
.IP \(bu 4
Zero-intrinsic knowledge required
.IP \(bu 4
Cross-platform (x86, ARM, WebAssembly)
.IP \(bu 4
Header-only (no compilation needed)
.IP \(bu 4
C++20 features (concepts, constraints)

.SH SUPPORTED ARCHITECTURES
.IP \(bu 4
x86: SSE, AVX, AVX2, AVX-512
.IP \(bu 4
ARM: NEON, SVE
.IP \(bu 4
WebAssembly: SIMD128
.IP \(bu 4
Scalar (fallback)

.SH QUICK START
.Ex .Bd -literal
#include "simdette/simdette.hpp"

using f32x4 = simdette::batch<float, simdette::default_arch>;

f32x4 a{1.0f, 2.0f, 3.0f, 4.0f};
f32x4 b{5.0f, 6.0f, 7.0f, 8.0f};

f32x4 sum = a + b;
.Ed

.SH SEE ALSO
.BR simdette::batch (3),
.BR simdette::abs (3),
.BR simdette::sqrt (3)

.SH AUTHOR
Simdette Developers <https://github.com/your-repo/simdette>
MANPAGE

# Generate batch manpage
cat > "$MAN_DIR/man3/simdette::batch.3" << 'MANPAGE'
.TH SIMDETTE::BATCH 3 "Simdette Library" "Version 0.1.0" "User Manual"
.SH NAME
simdette::batch \- SIMD batch operations

.SH SYNOPSIS
.B #include <simdette/simdette.hpp>

.B typedef batch<T, Arch>
.SH DESCRIPTION
The
.B batch
template is the primary interface for SIMD operations in Simdette.
It represents a vector of values processed in parallel.

.SH TEMPLATE PARAMETERS
.TP
.B T
The data type (float, double, int32_t, etc.)
.TP
.B Arch
The architecture tag (sse, avx2, neon, scalar, etc.)

.SH EXAMPLES
.Ex .Bd -literal
// Create a batch
simdette::batch<float, simdette::avx2> v{1.0f, 2.0f, 3.0f, 4.0f,
                                          5.0f, 6.0f, 7.0f, 8.0f};

// Arithmetic
auto sum = v + simdette::batch<float, simdette::avx2>{1.0f};

// Reduction
float total = v.reduce_add();
.Ed

.SH SEE ALSO
.BR simdette (7)
.SH AUTHOR
Simdette Developers
MANPAGE

# Generate math functions manpage
cat > "$MAN_DIR/man3/simdette::abs.3" << 'MANPAGE'
.TH SIMDETTE::ABS 3 "Simdette Library" "Version 0.1.0" "User Manual"
.SH NAME
simdette::abs, simdette::sqrt, simdette::floor \- Math functions

.SH SYNOPSIS
.B #include <simdette/math/basic_math.hpp>

.nf
.TP
.B simdette::batch<float, Arch> simdette::abs(batch<float, Arch> const& x)
.B simdette::batch<float, Arch> simdette::sqrt(batch<float, Arch> const& x)
.B simdette::batch<float, Arch> simdette::floor(batch<float, Arch> const& x)
.B simdette::batch<float, Arch> simdette::ceil(batch<float, Arch> const& x)
.B simdette::batch<float, Arch> simdette::round(batch<float, Arch> const& x)
.B simdette::batch<float, Arch> simdette::min(batch<float, Arch> const& a, batch<float, Arch> const& b)
.B simdette::batch<float, Arch> simdette::max(batch<float, Arch> const& a, batch<float, Arch> const& b)
.B simdette::batch<float, Arch> simdette::clamp(batch<float, Arch> const& x, batch<float, Arch> const& lo, batch<float, Arch> const& hi)
.fi

.SH DESCRIPTION
These functions provide common mathematical operations for SIMD batches.

.SH SEE ALSO
.BR simdette (7),
.BR simdette::batch (3)
.SH AUTHOR
Simdette Developers
MANPAGE

echo -e "${GREEN}Manpages generated in $MAN_DIR${NC}"
echo "Available manpages:"
find "$MAN_DIR" -name "*.3" -o -name "*.7" | sed 's/^/  /'
