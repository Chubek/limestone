// ============================================================================
// Simdette Example: Basic Usage
// ============================================================================
// This example demonstrates the fundamental operations of Simdette:
// - Batch creation and initialization
// - Arithmetic operations
// - Comparison operations
// - Memory access patterns
// ============================================================================

#include "simdette/simdette.hpp"
#include <iostream>
#include <vector>
#include <cmath>
#include <iomanip>

// Helper function to print a batch
template <typename T, typename Arch>
void print_batch(const simdette::batch<T, Arch>& v, const char* name) {
    constexpr std::size_t width = simdette::detail::batch_width_v<T, Arch>;
    alignas(32) std::array<T, width> data;
    v.to_array(data.data());

    std::cout << std::left << std::setw(10) << name << ": ";
    for (std::size_t i = 0; i < width; ++i) {
        std::cout << std::fixed << std::setprecision(2) << data[i];
        if (i < width - 1) std::cout << ", ";
    }
    std::cout << "\n";
}

int main() {
    std::cout << "=== Simdette Basic Operations ===\n\n";

    // ========================================================================
    // 1. Basic Batch Creation
    // ========================================================================
    std::cout << "1. Batch Creation\n";
    std::cout << "-----------------\n";

    using f32x4 = simdette::batch<float, simdette::default_arch>;

    // Default constructor (uninitialized)
    f32x4 a_default;

    // Broadcast scalar
    f32x4 a_broadcast(3.14f);
    print_batch(a_broadcast, "a_broadcast");

    // Load from array
    float arr[] = {1.0f, 2.0f, 3.0f, 4.0f};
    f32x4 a_load(arr);
    print_batch(a_load, "a_load");

    std::cout << "\n";

    // ========================================================================
    // 2. Arithmetic Operations
    // ========================================================================
    std::cout << "2. Arithmetic Operations\n";
    std::cout << "-------------------------\n";

    f32x4 a{1.0f, 2.0f, 3.0f, 4.0f};
    f32x4 b{5.0f, 6.0f, 7.0f, 8.0f};

    print_batch(a, "a");
    print_batch(b, "b");

    auto sum = a + b;
    print_batch(sum, "a + b");

    auto diff = a - b;
    print_batch(diff, "a - b");

    auto prod = a * b;
    print_batch(prod, "a * b");

    auto quot = a / b;
    print_batch(quot, "a / b");

    // Compound assignment
    f32x4 c = a;
    c += b;
    print_batch(c, "c += b");

    std::cout << "\n";

    // ========================================================================
    // 3. Scalar Operations
    // ========================================================================
    std::cout << "3. Scalar Operations\n";
    std::cout << "---------------------\n";

    auto scaled = a * 2.0f;
    print_batch(scaled, "a * 2.0f");

    auto offset = a + 1.0f;
    print_batch(offset, "a + 1.0f");

    auto divided = b / 2.0f;
    print_batch(divided, "b / 2.0f");

    std::cout << "\n";

    // ========================================================================
    // 4. Comparison Operations
    // ========================================================================
    std::cout << "4. Comparison Operations\n";
    std::cout << "-------------------------\n";

    f32x4 x{1.0f, 5.0f, 3.0f, 7.0f};
    f32x4 y{4.0f, 2.0f, 3.0f, 8.0f};

    print_batch(x, "x");
    print_batch(y, "y");

    auto lt = x < y;
    alignas(32) std::array<float, 4> lt_data;
    lt.to_array(lt_data.data());
    std::cout << "x < y: ";
    for (float v : lt_data) {
        std::cout << (v != 0 ? "1 " : "0 ");
    }
    std::cout << "\n";

    auto gt = x > y;
    gt.to_array(lt_data.data());
    std::cout << "x > y: ";
    for (float v : lt_data) {
        std::cout << (v != 0 ? "1 " : "0 ");
    }
    std::cout << "\n";

    auto eq = x == y;
    eq.to_array(lt_data.data());
    std::cout << "x == y: ";
    for (float v : lt_data) {
        std::cout << (v != 0 ? "1 " : "0 ");
    }
    std::cout << "\n";

    std::cout << "\n";

    // ========================================================================
    // 5. Reduction Operations
    // ========================================================================
    std::cout << "5. Reduction Operations\n";
    std::cout << "------------------------\n";

    f32x4 v{1.0f, 2.0f, 3.0f, 4.0f};

    float sum_reduce = v.reduce_add();
    float min_reduce = v.reduce_min();
    float max_reduce = v.reduce_max();

    std::cout << "Vector: [1, 2, 3, 4]\n";
    std::cout << "Sum (reduce_add): " << sum_reduce << "\n";
    std::cout << "Min (reduce_min): " << min_reduce << "\n";
    std::cout << "Max (reduce_max): " << max_reduce << "\n";

    std::cout << "\n";

    // ========================================================================
    // 6. Memory Access
    // ========================================================================
    std::cout << "6. Memory Access\n";
    std::cout << "-----------------\n";

    alignas(32) float data[4] = {10.0f, 20.0f, 30.0f, 40.0f};

    // Load aligned
    auto v_aligned = f32x4::load_aligned(data);
    print_batch(v_aligned, "load_aligned");

    // Load unaligned
    float unaligned[] = {5.0f, 6.0f, 7.0f, 8.0f};
    auto v_unaligned = f32x4::load_unaligned(unaligned);
    print_batch(v_unaligned, "load_unaligned");

    // Extract single element
    float first = v_aligned.extract(0);
    std::cout << "First element: " << first << "\n";

    // Store to array
    alignas(32) std::array<float, 4> out;
    v_aligned.to_array(out.data());
    std::cout << "Stored array: [";
    for (std::size_t i = 0; i < 4; ++i) {
        std::cout << out[i];
        if (i < 3) std::cout << ", ";
    }
    std::cout << "]\n";

    std::cout << "\n";

    // ========================================================================
    // 7. Different Data Types
    // ========================================================================
    std::cout << "7. Different Data Types\n";
    std::cout << "------------------------\n";

    // Double precision
    using f64x2 = simdette::batch<double, simdette::default_arch>;
    f64x2 d{1.5, 2.5};
    alignas(32) std::array<double, 2> d_data;
    d.to_array(d_data.data());
    std::cout << "Double batch: [" << d_data[0] << ", " << d_data[1] << "]\n";

    // Integer types
    using i32x4 = simdette::batch<int32_t, simdette::default_arch>;
    i32x4 i{1, 2, 3, 4};
    alignas(32) std::array<int32_t, 4> i_data;
    i.to_array(i_data.data());
    std::cout << "Int batch: [" << i_data[0] << ", " << i_data[1] 
              << ", " << i_data[2] << ", " << i_data[3] << "]\n";

    std::cout << "\n";

    // ========================================================================
    // Summary
    // ========================================================================
    std::cout << "=== All Basic Operations Complete ===\n";

    return 0;
}
