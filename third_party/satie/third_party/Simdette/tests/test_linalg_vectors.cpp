#include "simdette/math/linalg.hpp"

#include "AzmaTest/AzmaUnit.h"

#include <type_traits>

namespace {

using simdette::fvec2;
using simdette::fvec3;
using simdette::fvec4;

constexpr float kEps = 1.0e-5F;

[[nodiscard]] bool approx(float a, float b, float eps = kEps) {
    return simdette::abs(a - b) <= eps;
}

AZMA_TEST(test_vec2_arithmetic_operators) {
    constexpr simdette::vec2<int> a{1, 2};
    constexpr simdette::vec2<int> b{3, 4};

    constexpr auto sum = a + b;
    constexpr auto diff = b - a;
    constexpr auto scaled = 3 * a;

    constexpr simdette::vec2<int> expected_sum{4, 6};
    constexpr simdette::vec2<int> expected_diff{2, 2};
    constexpr simdette::vec2<int> expected_scaled{3, 6};

    AZMA_EXPECT(sum == expected_sum);
    AZMA_EXPECT(diff == expected_diff);
    AZMA_EXPECT(scaled == expected_scaled);
}

AZMA_TEST(test_vec3_dot_cross_length) {
    constexpr simdette::vec3<float> a{1.0F, 2.0F, 3.0F};
    constexpr simdette::vec3<float> b{4.0F, -5.0F, 6.0F};

    constexpr float d = simdette::dot(a, b);
    constexpr auto c = simdette::cross(a, b);

    AZMA_EXPECT(approx(d, 12.0F));
    AZMA_EXPECT(approx(c.x, 27.0F));
    AZMA_EXPECT(approx(c.y, 6.0F));
    AZMA_EXPECT(approx(c.z, -13.0F));
    AZMA_EXPECT(approx(simdette::length_squared(a), simdette::dot(a, a)));
}

AZMA_TEST(test_normalize_and_safe_normalize) {
    const fvec3 v{3.0F, 4.0F, 0.0F};
    const fvec3 n = simdette::normalize(v);

    AZMA_EXPECT(approx(simdette::length(n), 1.0F, 1.0e-4F));
    AZMA_EXPECT(approx(n.x, 0.6F, 1.0e-4F));
    AZMA_EXPECT(approx(n.y, 0.8F, 1.0e-4F));

    const fvec3 zero{0.0F};
    const fvec3 fallback{1.0F, 0.0F, 0.0F};
    const fvec3 safe = simdette::normalize_or(zero, fallback);

    AZMA_EXPECT(safe == fallback);
    AZMA_EXPECT(simdette::normalize(zero) == fvec3{0.0F});
}

AZMA_TEST(test_component_min_max_clamp) {
    const fvec4 a{-2.0F, 4.0F, 7.0F, -1.0F};
    const fvec4 b{3.0F, 1.0F, 8.0F, -5.0F};

    const fvec4 mn = simdette::min(a, b);
    const fvec4 mx = simdette::max(a, b);
    const fvec4 cl = simdette::clamp(a, fvec4{-1.0F}, fvec4{2.0F});

    const fvec4 expected_min{-2.0F, 1.0F, 7.0F, -5.0F};
    const fvec4 expected_max{3.0F, 4.0F, 8.0F, -1.0F};
    const fvec4 expected_clamp{-1.0F, 2.0F, 2.0F, -1.0F};

    AZMA_EXPECT(mn == expected_min);
    AZMA_EXPECT(mx == expected_max);
    AZMA_EXPECT(cl == expected_clamp);
}

AZMA_TEST(test_noexcept_and_constexpr_surface) {
    static_assert(noexcept(simdette::vec2<float>{}));
    static_assert(noexcept(simdette::dot(simdette::vec2<float>{}, simdette::vec2<float>{})));
    static_assert(std::is_trivially_copyable_v<simdette::vec4<double>>);

    constexpr auto v = simdette::vec3<int>{1, 2, 3};
    static_assert(v.x == 1);
    static_assert(v.z == 3);

    AZMA_EXPECT(true);
}

} // namespace

int main(int argc, char** argv) {
    return azma_unit_main(argc, argv);
}
