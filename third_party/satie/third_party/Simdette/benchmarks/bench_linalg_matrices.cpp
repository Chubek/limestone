#include "simdette/math/linalg.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

[[nodiscard]] simdette::fmat4 make_mat(std::uint32_t i) {
    const float f = static_cast<float>(i) * 0.001F;
    return simdette::fmat4{
        1.0F + f, 0.1F + f, 0.2F, 0.3F,
        0.0F, 1.0F + 0.5F * f, 0.4F, 0.1F,
        0.0F, 0.0F, 1.0F + 0.25F * f, 0.2F,
        0.0F, 0.0F, 0.0F, 1.0F,
    };
}

} // namespace

int main() {
    constexpr std::size_t kCount = 200000;
    std::vector<simdette::fmat4> mats;
    mats.reserve(kCount);
    for (std::size_t i = 0; i < kCount; ++i) {
        mats.push_back(make_mat(static_cast<std::uint32_t>(i)));
    }

    const simdette::fvec4 v{1.0F, 2.0F, 3.0F, 1.0F};

    auto start = std::chrono::high_resolution_clock::now();

    simdette::fmat4 accum = simdette::fmat4::identity();
    simdette::fvec4 out = v;

    for (std::size_t i = 0; i < kCount; ++i) {
        accum = mats[i] * accum;
        out = mats[i] * out;
    }

    auto end = std::chrono::high_resolution_clock::now();
    const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    std::cout << "linalg mat4 compose+apply: " << ns << " ns\n";
    std::cout << "checksum: " << accum(0, 0) + accum(1, 1) + accum(2, 2) + out.x + out.y + out.z + out.w << "\n";

    return 0;
}
