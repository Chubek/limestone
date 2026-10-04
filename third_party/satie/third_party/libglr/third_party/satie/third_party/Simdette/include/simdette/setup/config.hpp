#pragma once

// ============================================================================
// Compiler feature and inline attributes
// ============================================================================

#if defined(_MSC_VER)
#define SIMDETTE_ALWAYS_INLINE __forceinline
#define SIMDETTE_RESTRICT __restrict
#else
#define SIMDETTE_ALWAYS_INLINE __attribute__((always_inline))
#define SIMDETTE_RESTRICT __restrict__
#endif

// ============================================================================
// Architecture detection macros
// ============================================================================

#if defined(__AVX512F__)
#define SIMDETTE_ARCH_AVX512 1
#endif

#if defined(__AVX2__)
#define SIMDETTE_ARCH_AVX2 1
#endif

#if defined(__AVX__)
#define SIMDETTE_ARCH_AVX 1
#endif

#if defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && (_M_IX86_FP >= 2))
#define SIMDETTE_ARCH_SSE 1
#endif

#if defined(__ARM_FEATURE_SVE)
#define SIMDETTE_ARCH_SVE 1
#endif

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#define SIMDETTE_ARCH_NEON 1
#endif

#if defined(__wasm_simd128__)
#define SIMDETTE_ARCH_WASM_SIMD128 1
#endif
