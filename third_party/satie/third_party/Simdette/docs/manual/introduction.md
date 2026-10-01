# Introduction

## What is Simdette?

Simdette is a **header-only C++20 library** that provides a high-level
abstraction over SIMD (Single Instruction, Multiple Data) intrinsics. It
allows you to write vectorized code using familiar C++ operators,
without needing to know the underlying hardware intrinsics.

## Why Use Simdette?

Traditional SIMD programming requires:

-   Knowledge of architecture-specific intrinsics (SSE, AVX, NEON, etc.)
-   Manual handling of alignment and vectorization
-   Multiple code paths for different architectures
-   Verbose and error-prone code

Simdette solves these problems by:

-   **Abstracting intrinsics**: Write `a + b` instead of
    `_mm_add_ps(a, b)`
-   **Automatic architecture detection**: Your code runs optimally on
    any platform
-   **Clean syntax**: Use standard C++ operators for vectorized
    operations
-   **Type safety**: C++20 concepts ensure only compatible types are
    used

## Example: Matrix-Vector Multiplication

Without Simdette (using intrinsics):

``` cpp
#include <immintrin.h>

void dot_product(const float* a, const float* b, float* result, int n) {
    __m256 sum = _mm256_setzero_ps();
    for (int i = 0; i < n; i += 8) {
        __m256 va = _mm256_loadu_ps(a + i);
        __m256 vb = _mm256_loadu_ps(b + i);
        sum = _mm256_add_ps(sum, _mm256_mul_ps(va, vb));
    }
    // Horizontal sum...
}
```

With Simdette:

``` cpp
#include "simdette/simdette.hpp"

void dot_product(const float* a, const float* b, float* result, int n) {
    using f32x8 = simdette::batch<float, simdette::default_arch>;
    f32x8 sum(0.0f);
    for (int i = 0; i < n; i += 8) {
        f32x8 va(a + i);
        f32x8 vb(b + i);
        sum = sum + (va * vb);
    }
    *result = sum.reduce_add();
}
```

## Architecture Support

Simdette supports the following architectures:

**x86 Family**

-   **SSE**: Basic 128-bit SIMD (all modern x86/x86_64)
-   **AVX**: Advanced 256-bit SIMD
-   **AVX2**: AVX with integer support
-   **AVX-512**: 512-bit wide vectors (Intel Skylake+, AMD Zen4+)

**ARM Family**

-   **NEON**: ARM 128-bit SIMD (ARMv7 and later)
-   **SVE**: Scalable Vector Extension (ARMv8.2+)

**WebAssembly**

-   **SIMD128**: WebAssembly SIMD extension

**Fallback**

-   **Scalar**: Pure C++ fallback for any platform

## Performance Characteristics

| Architecture \| Vector Width \| Best For \|

\-\-\-\-\-\-\-\-\-\-\-\-\-- \| SSE \| 4 floats (128-bit) \| Legacy
systems \| \| AVX \| 8 floats (256-bit) \| Modern x86 \| \| AVX2 \| 8
floats + 8 ints (256-bit) \| Mixed workloads \| \| AVX-512 \| 16 floats
(512-bit) \| High-throughput \| \| NEON \| 4 floats (128-bit) \| ARM
mobile \| \| SVE \| Variable (128-2048-bit) \| ARM server \| \| Scalar
\| 1 float \| Universal fallback \|

## Getting Help

-   **Documentation**: See the generated Doxygen docs in `docs/doxygen/`
-   **Examples**: Check the `examples/` directory
-   **Tests**: Review the `tests/` directory for usage patterns

## Next Steps

-   `getting-started`{.interpreted-text role="doc"}: Set up your project
-   `core-concepts`{.interpreted-text role="doc"}: Understand the batch
    abstraction
-   `examples`{.interpreted-text role="doc"}: See real-world usage
