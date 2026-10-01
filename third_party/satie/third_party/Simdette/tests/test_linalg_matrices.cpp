#include "simdette/math/linalg.hpp"

#include "AzmaTest/AzmaUnit.h"

namespace {

constexpr float kEps = 1.0e-4F;

[[nodiscard]] bool approx(float a, float b, float eps = kEps) {
    return simdette::abs(a - b) <= eps;
}

AZMA_TEST(test_identity_and_mat_vec) {
    const simdette::fmat3 identity = simdette::fmat3::identity();
    const simdette::fvec3 vector{2.0F, -1.0F, 4.0F};
    const simdette::fvec3 mapped = identity * vector;

    AZMA_EXPECT(mapped == vector);
}

AZMA_TEST(test_mat_mat_compose_and_transpose_involution) {
    const simdette::fmat3 a{
        1.0F, 2.0F, 3.0F,
        0.0F, 1.0F, 4.0F,
        5.0F, 6.0F, 0.0F,
    };

    const simdette::fmat3 b{
        -2.0F, 1.0F, 0.0F,
        3.0F, 0.0F, 1.0F,
        4.0F, -1.0F, 2.0F,
    };

    const simdette::fmat3 c = a * b;
    const simdette::fmat3 expected{
        16.0F, -2.0F, 8.0F,
        19.0F, -4.0F, 9.0F,
        8.0F, 5.0F, 6.0F,
    };

    AZMA_EXPECT(c == expected);

    const simdette::fmat3 t2 = simdette::transpose(simdette::transpose(a));
    AZMA_EXPECT(t2 == a);
}

AZMA_TEST(test_determinant_inverse_mat2_mat3) {
    const simdette::fmat2 m2{
        4.0F, 7.0F,
        2.0F, 6.0F,
    };
    const float det2 = simdette::determinant(m2);
    AZMA_EXPECT(approx(det2, 10.0F));

    const simdette::fmat2 inv2 = simdette::inverse(m2);
    const simdette::fmat2 i2 = inv2 * m2;
    AZMA_EXPECT(approx(i2(0, 0), 1.0F));
    AZMA_EXPECT(approx(i2(0, 1), 0.0F));
    AZMA_EXPECT(approx(i2(1, 0), 0.0F));
    AZMA_EXPECT(approx(i2(1, 1), 1.0F));

    const simdette::fmat3 m3{
        1.0F, 2.0F, 3.0F,
        0.0F, 1.0F, 4.0F,
        5.0F, 6.0F, 0.0F,
    };
    const float det3 = simdette::determinant(m3);
    AZMA_EXPECT(approx(det3, 1.0F));

    const simdette::fmat3 inv3 = simdette::inverse(m3);
    const simdette::fmat3 i3 = inv3 * m3;
    AZMA_EXPECT(approx(i3(0, 0), 1.0F));
    AZMA_EXPECT(approx(i3(1, 1), 1.0F));
    AZMA_EXPECT(approx(i3(2, 2), 1.0F));
}

AZMA_TEST(test_inverse_mat4_and_singular_guard) {
    const simdette::fmat4 m{
        1.0F, 2.0F, 3.0F, 4.0F,
        0.0F, 1.0F, 4.0F, 2.0F,
        5.0F, 6.0F, 0.0F, 1.0F,
        0.0F, 0.0F, 0.0F, 1.0F,
    };

    const simdette::fmat4 inv = simdette::inverse(m);
    const simdette::fmat4 id = inv * m;

    AZMA_EXPECT(approx(id(0, 0), 1.0F, 1.0e-3F));
    AZMA_EXPECT(approx(id(1, 1), 1.0F, 1.0e-3F));
    AZMA_EXPECT(approx(id(2, 2), 1.0F, 1.0e-3F));
    AZMA_EXPECT(approx(id(3, 3), 1.0F, 1.0e-3F));

    const simdette::fmat2 singular{
        1.0F, 2.0F,
        2.0F, 4.0F,
    };
    const simdette::fmat2 inv_singular = simdette::inverse(singular);

    AZMA_EXPECT(inv_singular == simdette::fmat2{});
}

AZMA_TEST(test_composition_helpers) {
    const simdette::fmat3 t = simdette::homogeneous_translation(simdette::fvec2{2.0F, 3.0F});
    const simdette::fmat3 s = simdette::homogeneous_scale(simdette::fvec2{4.0F, 5.0F});
    const simdette::fmat3 r = simdette::homogeneous_rotation(0.0F);

    const simdette::fvec3 point{1.0F, 1.0F, 1.0F};
    const simdette::fvec3 out = (t * s * r) * point;

    AZMA_EXPECT(approx(out.x, 6.0F));
    AZMA_EXPECT(approx(out.y, 8.0F));
    AZMA_EXPECT(approx(out.z, 1.0F));

    const simdette::fmat3 basis = simdette::basis_from_columns(
        simdette::fvec3{1.0F, 0.0F, 0.0F},
        simdette::fvec3{0.0F, 1.0F, 0.0F},
        simdette::fvec3{0.0F, 0.0F, 1.0F});

    AZMA_EXPECT(basis == simdette::fmat3::identity());
}

} // namespace

int main(int argc, char** argv) {
    return azma_unit_main(argc, argv);
}
