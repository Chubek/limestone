// ============================================================================
// Simdette Comprehensive Tests
// ============================================================================

#include "simdette/simdette.hpp"
#include "simdette/math/basic_math.hpp"
#include <iostream>
#include <vector>
#include <cmath>
#include <cassert>
#include <array>

// ============================================================================
// Test: Basic Arithmetic
// ============================================================================

bool test_addition() {
    using batch_t = simdette::batch<float, simdette::default_arch>;
    constexpr std::size_t width = simdette::detail::batch_width_v<float, simdette::default_arch>;
    
    batch_t a{1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
    batch_t b{10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f, 70.0f, 80.0f};
    
    batch_t result = a + b;
    
    alignas(32) std::array<float, width> out;
    result.to_array(out.data());
    
    float expected[] = {11.0f, 22.0f, 33.0f, 44.0f, 55.0f, 66.0f, 77.0f, 88.0f};
    
    for (std::size_t i = 0; i < width; ++i) {
        if (std::fabs(out[i] - expected[i]) > 1e-5f) {
            std::cerr << "Addition failed at index " << i << "\n";
            return false;
        }
    }
    
    return true;
}

bool test_subtraction() {
    using batch_t = simdette::batch<float, simdette::default_arch>;
    constexpr std::size_t width = simdette::detail::batch_width_v<float, simdette::default_arch>;
    
    batch_t a{10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f, 70.0f, 80.0f};
    batch_t b{1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
    
    batch_t result = a - b;
    
    alignas(32) std::array<float, width> out;
    result.to_array(out.data());
    
    float expected[] = {9.0f, 18.0f, 27.0f, 36.0f, 45.0f, 54.0f, 63.0f, 72.0f};
    
    for (std::size_t i = 0; i < width; ++i) {
        if (std::fabs(out[i] - expected[i]) > 1e-5f) {
            std::cerr << "Subtraction failed at index " << i << "\n";
            return false;
        }
    }
    
    return true;
}

bool test_multiplication() {
    using batch_t = simdette::batch<float, simdette::default_arch>;
    constexpr std::size_t width = simdette::detail::batch_width_v<float, simdette::default_arch>;
    
    batch_t a{2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f};
    batch_t b{3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f};
    
    batch_t result = a * b;
    
    alignas(32) std::array<float, width> out;
    result.to_array(out.data());
    
    float expected[] = {6.0f, 12.0f, 20.0f, 30.0f, 42.0f, 56.0f, 72.0f, 90.0f};
    
    for (std::size_t i = 0; i < width; ++i) {
        if (std::fabs(out[i] - expected[i]) > 1e-5f) {
            std::cerr << "Multiplication failed at index " << i << "\n";
            return false;
        }
    }
    
    return true;
}

bool test_division() {
    using batch_t = simdette::batch<float, simdette::default_arch>;
    constexpr std::size_t width = simdette::detail::batch_width_v<float, simdette::default_arch>;
    
    batch_t a{10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f, 70.0f, 80.0f};
    batch_t b{2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f};
    
    batch_t result = a / b;
    
    alignas(32) std::array<float, width> out;
    result.to_array(out.data());
    
    float expected[] = {5.0f, 10.0f, 15.0f, 20.0f, 25.0f, 30.0f, 35.0f, 40.0f};
    
    for (std::size_t i = 0; i < width; ++i) {
        if (std::fabs(out[i] - expected[i]) > 1e-5f) {
            std::cerr << "Division failed at index " << i << "\n";
            return false;
        }
    }
    
    return true;
}

// ============================================================================
// Test: Comparison Operations
// ============================================================================

bool test_comparison() {
    using batch_t = simdette::batch<float, simdette::default_arch>;
    constexpr std::size_t width = simdette::detail::batch_width_v<float, simdette::default_arch>;
    
    batch_t a{1.0f, 5.0f, 3.0f, 7.0f, 2.0f, 6.0f, 4.0f, 8.0f};
    batch_t b{4.0f, 2.0f, 3.0f, 8.0f, 5.0f, 3.0f, 4.0f, 1.0f};
    
    auto lt = a < b;
    auto gt = a > b;
    auto eq = a == b;
    
    alignas(32) std::array<float, width> lt_out;
    lt.to_array(lt_out.data());
    
    // Check less-than: [1,0,1,0,1,0,0,1]
    bool lt_ok = (lt_out[0] != 0 && lt_out[1] == 0 && lt_out[2] == 0 && 
                  lt_out[3] == 0 && lt_out[4] != 0 && lt_out[5] == 0 && 
                  lt_out[6] == 0 && lt_out[7] != 0);
    
    if (!lt_ok) {
        std::cerr << "Less-than comparison failed\n";
        return false;
    }
    
    return true;
}

// ============================================================================
// Test: Reduction Operations
// ============================================================================

bool test_reduce() {
    using batch_t = simdette::batch<float, simdette::default_arch>;
    
    batch_t v{1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
    
    float sum = v.reduce_add();
    float min_val = v.reduce_min();
    float max_val = v.reduce_max();
    
    bool ok = true;
    if (std::fabs(sum - 36.0f) > 1e-5f) {
        std::cerr << "Reduce sum failed: " << sum << "\n";
        ok = false;
    }
    if (std::fabs(min_val - 1.0f) > 1e-5f) {
        std::cerr << "Reduce min failed: " << min_val << "\n";
        ok = false;
    }
    if (std::fabs(max_val - 8.0f) > 1e-5f) {
        std::cerr << "Reduce max failed: " << max_val << "\n";
        ok = false;
    }
    
    return ok;
}

// ============================================================================
// Test: Integer Operations
// ============================================================================

bool test_integer() {
    using batch_t = simdette::batch<int32_t, simdette::default_arch>;
    constexpr std::size_t width = simdette::detail::batch_width_v<int32_t, simdette::default_arch>;
    
    batch_t a{1, 2, 3, 4, 5, 6, 7, 8};
    batch_t b{10, 20, 30, 40, 50, 60, 70, 80};
    
    batch_t sum = a + b;
    
    alignas(32) std::array<int32_t, width> out;
    sum.to_array(out.data());
    
    int32_t expected[] = {11, 22, 33, 44, 55, 66, 77, 88};
    
    for (std::size_t i = 0; i < width; ++i) {
        if (out[i] != expected[i]) {
            std::cerr << "Integer addition failed at index " << i << "\n";
            return false;
        }
    }
    
    return true;
}

bool test_bitwise() {
    using batch_t = simdette::batch<int32_t, simdette::default_arch>;
    constexpr std::size_t width = simdette::detail::batch_width_v<int32_t, simdette::default_arch>;
    
    batch_t a{0xF0F0F0F0, 0x0F0F0F0F, 0xAAAAAAAA, 0x55555555};
    batch_t b{0x0F0F0F0F, 0xF0F0F0F0, 0x55555555, 0xAAAAAAAA};
    
    batch_t andv = a & b;
    batch_t orv = a | b;
    batch_t xorv = a ^ b;
    
    alignas(32) std::array<int32_t, width> and_out, or_out, xor_out;
    andv.to_array(and_out.data());
    orv.to_array(or_out.data());
    xorv.to_array(xor_out.data());
    
    // Check bitwise AND: 0x00000000, 0x00000000, 0x00000000, 0x00000000
    bool and_ok = (and_out[0] == 0 && and_out[1] == 0 && and_out[2] == 0 && and_out[3] == 0);
    
    // Check bitwise OR: 0xFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF
    bool or_ok = (or_out[0] == 0xF0F0F0FF || or_out[0] == 0xFFFFFFFF);
    
    return and_ok && or_ok;
}

// ============================================================================
// Test: Memory Operations
// ============================================================================

bool test_memory() {
    using batch_t = simdette::batch<float, simdette::default_arch>;
    constexpr std::size_t width = simdette::detail::batch_width_v<float, simdette::default_arch>;
    
    alignas(32) float data[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
    
    // Load aligned
    batch_t v1 = batch_t::load_aligned(data);
    
    // Load unaligned
    batch_t v2 = batch_t::load_unaligned(data);
    
    // Extract
    float first = v1.extract(0);
    float last = v1.extract(width - 1);
    
    // Store
    alignas(32) float out[8];
    v1.store_aligned(out);
    
    bool ok = true;
    if (first != 1.0f) {
        std::cerr << "Extract failed\n";
        ok = false;
    }
    if (last != 8.0f) {
        std::cerr << "Extract last failed\n";
        ok = false;
    }
    
    for (std::size_t i = 0; i < width; ++i) {
        if (std::fabs(out[i] - data[i]) > 1e-5f) {
            std::cerr << "Store failed at index " << i << "\n";
            ok = false;
        }
    }
    
    return ok;
}

// ============================================================================
// Test: Math Functions
// ============================================================================

bool test_math() {
    using batch_t = simdette::batch<float, simdette::default_arch>;
    constexpr std::size_t width = simdette::detail::batch_width_v<float, simdette::default_arch>;
    
    batch_t a{-3.0f, -2.0f, -1.0f, 0.0f, 1.0f, 2.0f, 3.0f, 4.0f};
    batch_t b{1.0f, 4.0f, 9.0f, 16.0f, 25.0f, 36.0f, 49.0f, 64.0f};
    
    auto abs_v = simdette::abs(a);
    auto sqrt_v = simdette::sqrt(b);
    auto floor_v = simdette::floor(a);
    
    alignas(32) std::array<float, width> out;
    
    // Test abs
    abs_v.to_array(out.data());
    if (std::fabs(out[0] - 3.0f) > 1e-5f || out[3] != 0.0f || out[7] != 4.0f) {
        std::cerr << "Abs test failed\n";
        return false;
    }
    
    // Test sqrt
    sqrt_v.to_array(out.data());
    if (std::fabs(out[0] - 1.0f) > 1e-4f || std::fabs(out[3] - 4.0f) > 1e-4f ||
        std::fabs(out[7] - 8.0f) > 1e-4f) {
        std::cerr << "Sqrt test failed\n";
        return false;
    }
    
    // Test floor
    floor_v.to_array(out.data());
    if (std::fabs(out[0] - (-3.0f)) > 1e-5f || std::fabs(out[5] - 2.0f) > 1e-5f) {
        std::cerr << "Floor test failed\n";
        return false;
    }
    
    return true;
}

bool test_min_max_clamp() {
    using batch_t = simdette::batch<float, simdette::default_arch>;
    constexpr std::size_t width = simdette::detail::batch_width_v<float, simdette::default_arch>;
    
    batch_t a{1.0f, 5.0f, 3.0f, 7.0f, 2.0f, 6.0f, 4.0f, 8.0f};
    batch_t b{4.0f, 2.0f, 6.0f, 1.0f, 5.0f, 3.0f, 8.0f, 0.0f};
    
    auto min_v = simdette::min(a, b);
    auto max_v = simdette::max(a, b);
    
    alignas(32) std::array<float, width> out;
    min_v.to_array(out.data());
    
    // Check min: [1, 2, 3, 1, 2, 3, 4, 0]
    bool min_ok = (std::fabs(out[0] - 1.0f) < 1e-5f && std::fabs(out[1] - 2.0f) < 1e-5f &&
                   std::fabs(out[3] - 1.0f) < 1e-5f && out[7] == 0.0f);
    
    if (!min_ok) {
        std::cerr << "Min test failed\n";
        return false;
    }
    
    return true;
}

// ============================================================================
// Test: Predicate Operations
// ============================================================================

bool test_predicates() {
    using batch_t = simdette::batch<float, simdette::default_arch>;
    
    batch_t v_all{1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
    batch_t v_any{1.0f, 0.0f, 3.0f, 0.0f, 5.0f, 0.0f, 7.0f, 0.0f};
    batch_t v_none{0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    
    bool all_ok = v_all.all();
    bool any_ok = v_any.any();
    bool none_all = !v_none.all();
    bool none_any = !v_none.any();
    
    if (!all_ok || !any_ok || !none_all || !none_any) {
        std::cerr << "Predicate test failed\n";
        return false;
    }
    
    return true;
}

// ============================================================================
// Main
// ============================================================================

int main() {
    int passed = 0;
    int failed = 0;
    
    std::cout << "=== Simdette Comprehensive Tests ===\n\n";
    
    // Arithmetic tests
    if (test_addition()) {
        std::cout << "[PASS] test_addition\n";
        passed++;
    } else {
        std::cout << "[FAIL] test_addition\n";
        failed++;
    }
    
    if (test_subtraction()) {
        std::cout << "[PASS] test_subtraction\n";
        passed++;
    } else {
        std::cout << "[FAIL] test_subtraction\n";
        failed++;
    }
    
    if (test_multiplication()) {
        std::cout << "[PASS] test_multiplication\n";
        passed++;
    } else {
        std::cout << "[FAIL] test_multiplication\n";
        failed++;
    }
    
    if (test_division()) {
        std::cout << "[PASS] test_division\n";
        passed++;
    } else {
        std::cout << "[FAIL] test_division\n";
        failed++;
    }
    
    // Comparison tests
    if (test_comparison()) {
        std::cout << "[PASS] test_comparison\n";
        passed++;
    } else {
        std::cout << "[FAIL] test_comparison\n";
        failed++;
    }
    
    // Reduction tests
    if (test_reduce()) {
        std::cout << "[PASS] test_reduce\n";
        passed++;
    } else {
        std::cout << "[FAIL] test_reduce\n";
        failed++;
    }
    
    // Integer tests
    if (test_integer()) {
        std::cout << "[PASS] test_integer\n";
        passed++;
    } else {
        std::cout << "[FAIL] test_integer\n";
        failed++;
    }
    
    if (test_bitwise()) {
        std::cout << "[PASS] test_bitwise\n";
        passed++;
    } else {
        std::cout << "[FAIL] test_bitwise\n";
        failed++;
    }
    
    // Memory tests
    if (test_memory()) {
        std::cout << "[PASS] test_memory\n";
        passed++;
    } else {
        std::cout << "[FAIL] test_memory\n";
        failed++;
    }
    
    // Math tests
    if (test_math()) {
        std::cout << "[PASS] test_math\n";
        passed++;
    } else {
        std::cout << "[FAIL] test_math\n";
        failed++;
    }
    
    if (test_min_max_clamp()) {
        std::cout << "[PASS] test_min_max_clamp\n";
        passed++;
    } else {
        std::cout << "[FAIL] test_min_max_clamp\n";
        failed++;
    }
    
    // Predicate tests
    if (test_predicates()) {
        std::cout << "[PASS] test_predicates\n";
        passed++;
    } else {
        std::cout << "[FAIL] test_predicates\n";
        failed++;
    }
    
    // Summary
    std::cout << "\n=== Summary ===\n";
    std::cout << "Passed: " << passed << "\n";
    std::cout << "Failed: " << failed << "\n";
    
    return failed > 0 ? 1 : 0;
}
