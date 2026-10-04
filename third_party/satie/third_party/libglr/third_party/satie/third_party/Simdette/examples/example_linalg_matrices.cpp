#include "simdette/math/linalg.hpp"

#include <iostream>

int main() {
    const simdette::fmat3 translate = simdette::homogeneous_translation(simdette::fvec2{2.0F, 3.0F});
    const simdette::fmat3 scale = simdette::homogeneous_scale(simdette::fvec2{4.0F, 2.0F});
    const simdette::fmat3 rotate = simdette::homogeneous_rotation(0.5F);

    const simdette::fmat3 composed = translate * rotate * scale;

    const simdette::fvec3 point{1.0F, -1.0F, 1.0F};
    const simdette::fvec3 mapped = composed * point;

    const simdette::fmat3 inv = simdette::inverse(composed);
    const simdette::fvec3 restored = inv * mapped;

    std::cout << "mapped: (" << mapped.x << ", " << mapped.y << ", " << mapped.z << ")\n";
    std::cout << "restored: (" << restored.x << ", " << restored.y << ", " << restored.z << ")\n";

    const simdette::fmat4 a = simdette::homogeneous_translation(simdette::fvec3{1.0F, 2.0F, 3.0F});
    const simdette::fmat4 b = simdette::homogeneous_scale(simdette::fvec3{2.0F, 2.0F, 2.0F});
    const simdette::fmat4 m = a * b;

    const simdette::fvec4 h{1.0F, 1.0F, 1.0F, 1.0F};
    const simdette::fvec4 h_out = m * h;

    std::cout << "homogeneous mapped: (" << h_out.x << ", " << h_out.y << ", " << h_out.z << ", " << h_out.w
              << ")\n";

    return 0;
}
