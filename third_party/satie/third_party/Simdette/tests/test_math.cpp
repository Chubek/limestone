#include "simdette/math/basic_math.hpp"
#include "simdette/simdette.hpp"
#include <iostream>
#include <vector>
#include <cmath>
#include <cassert>

bool test_sqrt() {
    constexpr std::size_t N = 1024;
    std::vector<float> a(N);
    std::vector<float> ref(N), out(N);

    for (std::size_t i = 0; i < N; ++i) {
        a[i] = 1.0f + static_cast<float>(i) / 100.0f;
        ref[i] = std::sqrt(a[i]);
    }

    using batch_t = simdette::batch<float, simdette::default_arch>;
    constexpr std::size_t width = simdette::detail::batch_width_v<float, simdette::default_arch>;
    std::size_t blocks = N / width;

    for (std::size_t block = 0; block < blocks; ++block) {
        batch_t va(a.data() + block * width);
        batch_t vr = simdette::sqrt(va);
        vr.to_array(out.data() + block * width);
    }

    bool ok = true;
    for (std::size_t i = 0; i < N; ++i) {
        if (std::fabs(out[i] - ref[i]) > 1e-4f) {
            ok = false;
            std::cerr << "sqrt mismatch at " << i << ": " << out[i] << " vs " << ref[i] << "\n";
        }
    }

    return ok;
}

bool test_abs() {
    std::vector<float> a{-1.0f, 2.0f, -3.0f, 4.0f};
    std::vector<float> ref{1.0f, 2.0f, 3.0f, 4.0f};

    using batch_t = simdette::batch<float, simdette::default_arch>;
    batch_t va(a.data());
    batch_t vr = simdette::abs(va);
    alignas(16) std::array<float, 4> res{};
    vr.to_array(res.data());

    bool ok = true;
    for (std::size_t i = 0; i < 4; ++i) {
        if (std::fabs(res[i] - ref[i]) > 1e-5f) {
            ok = false;
        }
    }

    return ok;
}

int main() {
    bool ok1 = test_sqrt();
    bool ok2 = test_abs();

    if (ok1 && ok2) {
        std::cout << "test_math: PASSED\n";
        return 0;
    } else {
        std::cerr << "test_math: FAILED\n";
        return 1;
    }
}
