#include "simdette/simdette.hpp"
#include <iostream>
#include <vector>
#include <chrono>
#include <cmath>

double benchmark_scalar_dot(std::vector<float>& a, std::vector<float>& b) {
    double sum = 0.0;
    for (size_t i = 0; i < a.size(); ++i) sum += a[i] * b[i];
    return sum;
}

double benchmark_simd_dot(std::vector<float>& a, std::vector<float>& b) {
    using batch_t = simdette::batch<float, simdette::default_arch>;
    constexpr std::size_t width = simdette::detail::batch_width_v<float, simdette::default_arch>;
    size_t N = a.size();
    size_t blocks = N / width;

    __m128 sum_vec = _mm_setzero_ps();
    for (size_t i = 0; i < blocks; ++i) {
        batch_t va(a.data() + i * width);
        batch_t vb(b.data() + i * width);
        batch_t prod = va * vb;
        sum_vec = _mm_add_ps(sum_vec, _mm_set_ps(
            prod.extract(0), prod.extract(1), prod.extract(2), prod.extract(3)
        ));
    }

    alignas(16) float res[4];
    _mm_store_ps(res, sum_vec);
    double sum = res[0] + res[1] + res[2] + res[3];
    return sum;
}

int main() {
    const size_t N = 1024 * 1024;
    std::vector<float> a(N, 1.0f), b(N, 2.0f);

    auto t0 = std::chrono::high_resolution_clock::now();
    double ref = benchmark_scalar_dot(a, b);
    auto t1 = std::chrono::high_resolution_clock::now();
    double simd = benchmark_simd_dot(a, b);
    auto t2 = std::chrono::high_resolution_clock::now();

    double scalar_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
    double simd_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count();

    std::cout << "scalar dot: " << scalar_ns << " ns\n";
    std::cout << "simd dot:   " << simd_ns << " ns\n";
    std::cout << "speedup:    " << (scalar_ns / simd_ns) << "x\n";
    std::cout << "error:      " << std::fabs(simd - ref) << "\n";

    return 0;
}
