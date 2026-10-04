#include "simdette/math/linalg.hpp"

#include <iostream>

int main() {
    const simdette::fvec3 a{1.0F, 2.0F, 3.0F};
    const simdette::fvec3 b{4.0F, 1.0F, -2.0F};

    const simdette::fvec3 sum = a + b;
    const simdette::fvec3 scaled = 0.5F * sum;
    const float d = simdette::dot(a, b);
    const simdette::fvec3 c = simdette::cross(a, b);
    const simdette::fvec3 n = simdette::normalize(scaled);

    std::cout << "sum: (" << sum.x << ", " << sum.y << ", " << sum.z << ")\n";
    std::cout << "scaled: (" << scaled.x << ", " << scaled.y << ", " << scaled.z << ")\n";
    std::cout << "dot: " << d << "\n";
    std::cout << "cross: (" << c.x << ", " << c.y << ", " << c.z << ")\n";
    std::cout << "normalized length: " << simdette::length(n) << "\n";

    return 0;
}
