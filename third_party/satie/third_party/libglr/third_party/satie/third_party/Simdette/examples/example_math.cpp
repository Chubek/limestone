// ============================================================================
// Simdette Example: Math Operations
// ============================================================================
// This example demonstrates mathematical functions provided by Simdette:
// - Elementary functions (abs, sqrt, etc.)
// - Transcendental functions
// - Min/max/clamp operations
// ============================================================================

#include "simdette/simdette.hpp"
#include "simdette/math/basic_math.hpp"
#include "simdette/math/transcendental.hpp"
#include <iostream>
#include <cmath>
#include <iomanip>
#include <limits>

template <typename T, typename Arch>
void print_batch(const simdette::batch<T, Arch>& v, const char* name, 
                 bool show_float = true) {
    constexpr std::size_t width = simdette::detail::batch_width_v<T, Arch>;
    alignas(32) std::array<T, width> data;
    v.to_array(data.data());

    std::cout << std::left << std::setw(15) << name << ": ";
    for (std::size_t i = 0; i < width; ++i) {
        if constexpr (std::is_floating_point_v<T>) {
            if (show_float) {
                std::cout << std::fixed << std::setprecision(4) << data[i];
            } else {
                std::cout << data[i];
            }
        } else {
            std::cout << data[i];
        }
        if (i < width - 1) std::cout << ", ";
    }
    std::cout << "\n";
}

int main() {
    std::cout << "=== Simdette Math Functions ===\n\n";

    using f32x4 = simdette::batch<float, simdette::default_arch>;

    // ========================================================================
    // 1. Elementary Functions
    // ========================================================================
    std::cout << "1. Elementary Functions\n";
    std::cout << "------------------------\n";

    f32x4 a{-3.0f, -2.0f, -1.0f, 0.0f, 1.0f, 2.0f, 3.0f, 4.0f};
    print_batch(a, "Input");

    auto abs_v = simdette::abs(a);
    print_batch(abs_v, "|a|");

    std::cout << "\n";

    // ========================================================================
    // 2. Square Root
    // ========================================================================
    std::cout << "2. Square Root\n";
    std::cout << "---------------\n";

    f32x4 b{1.0f, 4.0f, 9.0f, 16.0f, 25.0f, 36.0f, 49.0f, 64.0f};
    print_batch(b, "Input");

    auto sqrt_v = simdette::sqrt(b);
    print_batch(sqrt_v, "sqrt(b)");

    // Verify correctness
    alignas(32) std::array<float, 8> sqrt_data;
    sqrt_v.to_array(sqrt_data.data());
    bool correct = true;
    for (std::size_t i = 0; i < 8; ++i) {
        float expected = std::sqrt(b.extract(i));
        if (std::fabs(sqrt_data[i] - expected) > 1e-4f) {
            correct = false;
        }
    }
    std::cout << "Verification: " << (correct ? "PASSED" : "FAILED") << "\n\n";

    // ========================================================================
    // 3. Reciprocal Square Root
    // ========================================================================
    std::cout << "3. Reciprocal Square Root\n";
    std::cout << "--------------------------\n";

    f32x4 c{1.0f, 4.0f, 9.0f, 16.0f};
    print_batch(c, "Input");

    auto rsqrt_v = simdette::rsqrt(c);
    print_batch(rsqrt_v, "1/sqrt(c)");

    alignas(32) std::array<float, 8> rsqrt_data;
    rsqrt_v.to_array(rsqrt_data.data());
    std::cout << "Note: rsqrt is fast approximation\n\n";

    // ========================================================================
    // 4. Floor, Ceil, Round, Trunc
    // ========================================================================
    std::cout << "4. Rounding Functions\n";
    std::cout << "----------------------\n";

    f32x4 d{1.7f, 2.3f, 3.9f, 4.1f, -1.7f, -2.3f, -3.9f, -4.1f};
    print_batch(d, "Input");

    auto floored = simdette::floor(d);
    print_batch(floored, "floor");

    auto ceiled = simdette::ceil(d);
    print_batch(ceiled, "ceil");

    auto rounded = simdette::round(d);
    print_batch(rounded, "round");

    auto truncated = simdette::trunc(d);
    print_batch(truncated, "trunc");

    std::cout << "\n";

    // ========================================================================
    // 5. Min/Max/Clamp
    // ========================================================================
    std::cout << "5. Min/Max/Clamp\n";
    std::cout << "-----------------\n";

    f32x4 e{1.0f, 5.0f, 3.0f, 7.0f, 2.0f, 6.0f, 4.0f, 8.0f};
    f32x4 f{4.0f, 2.0f, 6.0f, 1.0f, 5.0f, 3.0f, 8.0f, 0.0f};

    print_batch(e, "e");
    print_batch(f, "f");

    auto min_v = simdette::min(e, f);
    print_batch(min_v, "min(e, f)");

    auto max_v = simdette::max(e, f);
    print_batch(max_v, "max(e, f)");

    // Clamp example
    f32x4 g{-10.0f, 0.0f, 5.0f, 10.0f, 15.0f, 20.0f, 25.0f, 30.0f};
    print_batch(g, "g (before clamp)");

    f32x4 lo{0.0f};
    f32x4 hi{10.0f};
    auto clamped = simdette::clamp(g, lo, hi);
    print_batch(clamped, "clamp(g, 0, 10)");

    std::cout << "\n";

    // ========================================================================
    // 6. Transcendental Functions
    // ========================================================================
    std::cout << "6. Transcendental Functions\n";
    std::cout << "----------------------------\n";

    f32x4 h{0.1f, 0.5f, 1.0f, 1.5f, 2.0f, 2.5f, 3.0f, 3.5f};
    print_batch(h, "Input");

    auto sin_v = simdette::sin(h);
    print_batch(sin_v, "sin(h)");

    auto cos_v = simdette::cos(h);
    print_batch(cos_v, "cos(h)");

    auto tan_v = simdette::tan(h);
    print_batch(tan_v, "tan(h)");

    std::cout << "\n";

    auto exp_v = simdette::exp(h);
    print_batch(exp_v, "exp(h)");

    auto log_v = simdette::log(h);
    print_batch(log_v, "log(h)");

    std::cout << "\n";

    // ========================================================================
    // 7. Special Values
    // ========================================================================
    std::cout << "7. Special Values\n";
    std::cout << "------------------\n";

    f32x4 special{1.0f, 2.0f, 
                  std::numeric_limits<float>::quiet_NaN(), 
                  std::numeric_limits<float>::infinity(),
                  -std::numeric_limits<float>::infinity(), 
                  4.0f, 5.0f, 6.0f};
    
    print_batch(special, "Special values");

    auto isnan = simdette::isnan(special);
    alignas(32) std::array<float, 8> isnan_data;
    isnan.to_array(isnan_data.data());
    std::cout << "Is NaN: ";
    for (float v : isnan_data) {
        std::cout << (v != 0 ? "1 " : "0 ");
    }
    std::cout << "\n";

    auto isinf = simdette::isinf(special);
    isinf.to_array(isnan_data.data());
    std::cout << "Is Inf: ";
    for (float v : isnan_data) {
        std::cout << (v != 0 ? "1 " : "0 ");
    }
    std::cout << "\n\n";

    // ========================================================================
    // 8. Performance Note
    // ========================================================================
    std::cout << "=== Math Functions Complete ===\n";
    std::cout << "\nNote: Math functions are implemented using\n";
    std::cout << "table lookup or polynomial approximation\n";
    std::cout << "for performance on SIMD architectures.\n";

    return 0;
}
