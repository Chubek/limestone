#include "simdette/simdette.hpp"
#include <iostream>
#include <vector>
#include <cmath>
#include <cassert>

bool test_float_addition() {
    constexpr std::size_t N = 1024;
    std::vector<float> a(N, 1.0f), b(N, 2.0f), c(N, 0.0f);
    std::vector<float> ref(N);

    // Scalar ref
    for (std::size_t i = 0; i < N; ++i) ref[i] = a[i] + b[i];

    // SIMD
    using batch_t = simdette::batch<float, simdette::default_arch>;
    constexpr std::size_t width = simdette::detail::batch_width_v<float, simdette::default_arch>;
    std::size_t blocks = N / width;

    for (std::size_t block = 0; block < blocks; ++block) {
        batch_t va(a.data() + block * width);
        batch_t vb(b.data() + block * width);
        batch_t vc = va + vb;
        vc.to_array(c.data() + block * width);
    }

    bool ok = true;
    for (std::size_t i = 0; i < N; ++i) {
        if (std::fabs(c[i] - ref[i]) > 1e-5f) {
            ok = false;
            std::cerr << "Mismatch at " << i << ": " << c[i] << " vs " << ref[i] << "\n";
        }
    }

    return ok;
}

int main() {
    if (test_float_addition()) {
        std::cout << "test_addition: PASSED\n";
        return 0;
    } else {
        std::cerr << "test_addition: FAILED\n";
        return 1;
    }
}
