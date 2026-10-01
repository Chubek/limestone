# Best Practices

This guide provides best practices for using Simdette effectively and
efficiently.

## Performance Optimization

\### 1. Choose the Right Architecture

Select the architecture that matches your target hardware:

``` bash
# For modern x86 (default choice)
g++ -std=c++20 -march=native -O3 ...

# For maximum compatibility
g++ -std=c++20 -msse2 -O3 ...

# For AVX2 workloads
g++ -std=c++20 -mavx2 -O3 ...
```

\### 2. Align Your Data

Always align data for optimal performance:

``` cpp
// Good: Aligned
alignas(32) float data[8];
auto v = batch_t::load_aligned(data);

// Avoid: Unaligned (slower)
float data[8];
auto v = batch_t::load_unaligned(data);
```

\### 3. Minimize Memory Access

Keep data in registers as long as possible:

``` cpp
// Good: Fused operations
auto result = (a * a) + (b * b);

// Avoid: Unnecessary stores
float tmp[8];
a.to_array(tmp);
// ... scalar ops ...
auto v = batch_t(tmp);
```

\### 4. Use Appropriate Data Types

Match your data type to the architecture:

``` cpp
// For float-heavy workloads
using f32x8 = simdette::batch<float, simdette::avx2>;

// For mixed workloads
using i32x8 = simdette::batch<int32_t, simdette::avx2>;
```

## Code Organization

\### 1. Separate Architecture-Specific Code

``` cpp
// architecture.hpp
#if defined(__AVX2__)
using default_arch = simdette::avx2;
#elif defined(__SSE2__)
using default_arch = simdette::sse;
#else
using default_arch = simdette::scalar;
#endif
```

\### 2. Use Templates for Generic Code

``` cpp
template <typename T, typename Arch = simdette::default_arch>
void process(const T* data, std::size_t n) {
    using batch_t = simdette::batch<T, Arch>;
    // Generic implementation
}
```

\### 3. Provide Scalar Fallback

``` cpp
#if defined(__AVX2__)
using simd_arch = simdette::avx2;
#else
using simd_arch = simdette::scalar;
#endif
```

## Testing and Debugging

\### 1. Write Comprehensive Tests

Test all operations including edge cases:

``` cpp
bool test_special_values() {
    using batch_t = simdette::batch<float, simdette::avx2>;

    batch_t inf(1.0f/0.0f);
    batch_t nan(std::numeric_limits<float>::quiet_NaN());

    // Test operations on special values
    auto sum = inf + inf;
    auto nan_result = nan + 1.0f;

    return simdette::isinf(sum).all() && simdette::isnan(nan_result).any();
}
```

\### 2. Use Verification Functions

Always verify SIMD results against scalar:

``` cpp
bool verify(const std::vector<float>& simd_result, 
            const std::vector<float>& scalar_result) {
    for (std::size_t i = 0; i < simd_result.size(); ++i) {
        if (std::fabs(simd_result[i] - scalar_result[i]) > 1e-5f) {
            std::cerr << "Mismatch at " << i << "\n";
            return false;
        }
    }
    return true;
}
```

\### 3. Test Multiple Architectures

Test on different architectures if possible:

``` bash
# Test on SSE
g++ -std=c++20 -msse2 -O2 tests/test_comprehensive.cpp -o test_sse

# Test on AVX2
g++ -std=c++20 -mavx2 -O2 tests/test_comprehensive.cpp -o test_avx2
```

## Memory Management

\### 1. Use aligned_array for RAII

``` cpp
#include "simdette/core/memory.hpp"

void process() {
    simdette::aligned_array<float> buffer(1024);
    // Automatically freed
}
```

\### 2. Avoid Unnecessary Allocations

Pre-allocate buffers for repeated operations:

``` cpp
class Processor {
    simdette::aligned_array<float> temp_buffer;
    std::size_t buffer_size;

public:
    Processor(std::size_t n) 
        : buffer_size(n), temp_buffer(n) {}
};
```

## Portability

\### 1. Use default_arch for Auto-Detection

``` cpp
// Best: Automatic detection
using batch_t = simdette::batch<float, simdette::default_arch>;
```

\### 2. Provide Runtime Dispatch for Multiple Architectures

``` cpp
void dot_product_dispatch(const float* a, const float* b, 
                         float* result, std::size_t n) {
#if defined(__AVX2__)
    dot_product_avx2(a, b, result, n);
#elif defined(__SSE2__)
    dot_product_sse(a, b, result, n);
#else
    dot_product_scalar(a, b, result, n);
#endif
}
```

\### 3. Handle Architecture-Specific Features Gracefully

``` cpp
#if defined(SIMDETTE_ARCH_AVX512)
// Use AVX-512 features
#endif
```

## Common Pitfalls

\### 1. Uninitialized Memory

Always initialize batches:

``` cpp
// Bad: Uninitialized
batch<float, simd_t> v;
auto result = v * other;  // Undefined behavior

// Good: Initialize
batch<float, simd_t> v(0.0f);
auto result = v * other;
```

\### 2. Misaligned Pointers

Ensure proper alignment for aligned operations:

``` cpp
// Bad: Potential misalignment
float* data = new float[8];
auto v = batch_t::load_aligned(data);  // May crash

// Good: Proper alignment
alignas(32) float* data = new float[8];
auto v = batch_t::load_aligned(data);
```

\### 3. Incorrect Vector Width Assumptions

Don\'t assume fixed vector width:

``` cpp
// Bad: Fixed width assumption
for (int i = 0; i < 8; ++i) {  // Assumes width=8
    // ...
}

// Good: Use batch width
constexpr std::size_t width = simdette::detail::batch_width_v<float, simd_t>;
for (int i = 0; i < width; ++i) {
    // ...
}
```

## Style Guidelines

\### 1. Use Type Aliases

``` cpp
using f32x4 = simdette::batch<float, simdette::default_arch>;
using f64x2 = simdette::batch<double, simdette::default_arch>;
```

\### 2. Follow C++ Conventions

Use standard naming and organization:

``` cpp
// Good: Clear naming
batch<float, simd_t> input_data;
batch<float, simd_t> output_result;

// Bad: Unclear naming
batch<float, simd_t> a, b;
```

\### 3. Document Complex Operations

``` cpp
// Compute (a + b)^2 = a^2 + 2ab + b^2
auto sq_sum = simdette::mul(a, a) + 
              simdette::mul(f32x4(2.0f), simdette::mul(a, b)) +
              simdette::mul(b, b);
```

## Error Handling

\### 1. Check for Special Values

``` cpp
void safe_divide(batch<float, simd_t>& a, batch<float, simd_t>& b) {
    auto non_zero = b != batch<float, simd_t>(0.0f);
    // Handle division by zero
}
```

\### 2. Use Assertions in Debug Builds

``` cpp
#ifndef NDEBUG
#define SIMD_ASSERT(cond) \
    do { if (!(cond)) std::abort(); } while(0)
#else
#define SIMD_ASSERT(cond)
#endif
```

\### 3. Validate Input Bounds

``` cpp
void validate_input(const float* data, std::size_t n) {
    SIMD_ASSERT(data != nullptr);
    SIMD_ASSERT(n > 0);
    SIMD_ASSERT(reinterpret_cast<std::uintptr_t>(data) % 32 == 0);
}
```

## Profiling and Debugging

\### 1. Use Compiler Optimizations

``` bash
# Debug build
g++ -std=c++20 -O0 -g ...

# Release build
g++ -std=c++20 -O3 -DNDEBUG ...
```

\### 2. Inspect Generated Assembly

``` bash
g++ -std=c++20 -S -O3 -march=native your_code.cpp
cat your_code.s
```

\### 3. Use Profiling Tools

``` bash
g++ -std=c++20 -pg -O3 your_code.cpp
gprof a.out
```

## Summary

Following these best practices will help you:

-   Write efficient SIMD code
-   Ensure portability across architectures
-   Debug SIMD issues effectively
-   Maintain clean, understandable code

Key Takeaways:

1.  **Align your data** for optimal performance
2.  **Use default_arch** for auto-detection
3.  **Test with verification** against scalar code
4.  **Initialize batches** to avoid undefined behavior
5.  **Profile and optimize** based on real workloads
