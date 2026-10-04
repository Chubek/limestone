// ============================================================================
// Simdette Example: Vector Dot Product
// ============================================================================
// This example demonstrates a practical SIMD application: computing the
// dot product of two vectors using Simdette.
// ============================================================================

#include "simdette/simdette.hpp"
#include <iostream>
#include <vector>
#include <cmath>
#include <chrono>
#include <numeric>

// ============================================================================
// Scalar Reference Implementation
// ============================================================================

float dot_product_scalar(const std::vector<float>& a, 
                         const std::vector<float>& b) {
    float sum = 0.0f;
    for (std::size_t i = 0; i < a.size(); ++i) {
        sum += a[i] * b[i];
    }
    return sum;
}

// ============================================================================
// SIMD Dot Product Implementation
// ============================================================================

float dot_product_simd(const std::vector<float>& a, 
                       const std::vector<float>& b) {
    using batch_t = simdette::batch<float, simdette::default_arch>;
    constexpr std::size_t width = simdette::detail::batch_width_v<float, simdette::default_arch>;
    
    std::size_t n = a.size();
    std::size_t blocks = n / width;
    
    batch_t sum(0.0f);
    
    // Process full blocks
    for (std::size_t block = 0; block < blocks; ++block) {
        batch_t va(a.data() + block * width);
        batch_t vb(b.data() + block * width);
        sum = sum + (va * vb);
    }
    
    // Handle remaining elements
    float remainder = 0.0f;
    for (std::size_t i = blocks * width; i < n; ++i) {
        remainder += a[i] * b[i];
    }
    
    // Reduce and add remainder
    return sum.reduce_add() + remainder;
}

// ============================================================================
// Benchmarking
// ============================================================================

void benchmark(const std::vector<float>& a, 
               const std::vector<float>& b,
               std::size_t iterations) {
    // Warmup
    float result_scalar = dot_product_scalar(a, b);
    float result_simd = dot_product_simd(a, b);
    
    // Scalar timing
    auto start = std::chrono::high_resolution_clock::now();
    for (std::size_t i = 0; i < iterations; ++i) {
        volatile float r = dot_product_scalar(a, b);
    }
    auto scalar_end = std::chrono::high_resolution_clock::now();
    
    // SIMD timing
    start = std::chrono::high_resolution_clock::now();
    for (std::size_t i = 0; i < iterations; ++i) {
        volatile float r = dot_product_simd(a, b);
    }
    auto simd_end = std::chrono::high_resolution_clock::now();
    
    auto scalar_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        scalar_end - start).count();
    auto simd_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        simd_end - start).count();
    
    std::cout << "Vector size: " << a.size() << " elements\n";
    std::cout << "Scalar time: " << scalar_ns << " ns\n";
    std::cout << "SIMD time:   " << simd_ns << " ns\n";
    std::cout << "Speedup:     " << (scalar_ns / static_cast<float>(simd_ns)) << "x\n";
    std::cout << "Results match: " << (result_scalar == result_simd ? "YES" : "NO") << "\n\n";
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "=== Simdette Dot Product Example ===\n\n";
    
    // Test correctness with small vectors
    std::cout << "Correctness Tests:\n";
    std::cout << "------------------\n";
    
    std::vector<float> test1 = {1.0f, 2.0f, 3.0f, 4.0f};
    std::vector<float> test2 = {5.0f, 6.0f, 7.0f, 8.0f};
    
    float scalar_result = dot_product_scalar(test1, test2);
    float simd_result = dot_product_simd(test1, test2);
    
    std::cout << "Test 1 (4 elements):\n";
    std::cout << "  Scalar: " << scalar_result << "\n";
    std::cout << "  SIMD:   " << simd_result << "\n";
    std::cout << "  Expected: 70.0\n";
    std::cout << "  Match: " << (std::fabs(scalar_result - simd_result) < 1e-5f ? "YES" : "NO") << "\n\n";
    
    // Test with different sizes
    std::vector<float> test2_sizes[] = {
        {1.0f, 2.0f, 3.0f, 4.0f, 5.0f},
        {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f},
        {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f}
    };
    
    for (std::size_t i = 0; i < 3; ++i) {
        scalar_result = dot_product_scalar(test2_sizes[i], test2_sizes[i]);
        simd_result = dot_product_simd(test2_sizes[i], test2_sizes[i]);
        
        std::cout << "Test " << (i + 2) << " (" << test2_sizes[i].size() << " elements):\n";
        std::cout << "  Scalar: " << scalar_result << "\n";
        std::cout << "  SIMD:   " << simd_result << "\n";
        std::cout << "  Match:  " << (std::fabs(scalar_result - simd_result) < 1e-5f ? "YES" : "NO") << "\n\n";
    }
    
    // Benchmark different sizes
    std::cout << "Performance Benchmarks:\n";
    std::cout << "-----------------------\n";
    
    std::vector<std::size_t> sizes = {1024, 4096, 16384, 65536};
    std::size_t iterations = 10000;
    
    for (std::size_t n : sizes) {
        std::vector<float> a(n), b(n);
        std::iota(a.begin(), a.end(), 1.0f);
        std::iota(b.begin(), b.end(), 2.0f);
        
        benchmark(a, b, iterations);
    }
    
    std::cout << "=== Dot Product Example Complete ===\n";
    
    return 0;
}
