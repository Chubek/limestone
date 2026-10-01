#pragma once

#include "simdette/setup/config.hpp"

namespace simdette {

// ============================================================================
// Architecture tags
// ============================================================================

struct scalar {};

// x86
struct sse {};
struct avx {};
struct avx2 {};
struct avx512 {};

// ARM
struct neon {};
struct sve {};

// WebAssembly
struct simd128 {};

// ============================================================================
// Compile-time architecture detection
// ============================================================================

#if defined(SIMDETTE_ARCH_AVX512)
using default_arch = avx512;
#elif defined(SIMDETTE_ARCH_AVX2)
using default_arch = avx2;
#elif defined(SIMDETTE_ARCH_AVX)
using default_arch = avx;
#elif defined(SIMDETTE_ARCH_SSE)
using default_arch = sse;
#elif defined(SIMDETTE_ARCH_SVE)
using default_arch = sve;
#elif defined(SIMDETTE_ARCH_NEON)
using default_arch = neon;
#elif defined(SIMDETTE_ARCH_WASM_SIMD128)
using default_arch = simd128;
#else
using default_arch = scalar;
#endif

} // namespace simdette
