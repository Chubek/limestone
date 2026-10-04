#include "simdette/math/linalg.hpp"

#include "AzmaTest/AzmaFuzz.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>

namespace {

constexpr float kTol = 2.0e-3F;

[[nodiscard]] bool finite(float value) {
    return std::isfinite(value);
}

[[nodiscard]] bool approx(float a, float b, float tol = kTol) {
    const float diff = simdette::abs(a - b);
    const float scale = simdette::max(1.0F, simdette::max(simdette::abs(a), simdette::abs(b)));
    return diff <= tol * scale;
}

[[nodiscard]] float sample(const uint8_t* data, std::size_t size, std::size_t offset) {
    if (size == 0) {
        return 0.0F;
    }

    const std::size_t i0 = offset % size;
    const std::size_t i1 = (offset + 1) % size;
    const uint16_t packed = (static_cast<uint16_t>(data[i0]) << 8) | static_cast<uint16_t>(data[i1]);
    const float n = static_cast<float>(packed) / 65535.0F;
    return n * 20.0F - 10.0F;
}

[[nodiscard]] simdette::fvec3 load_vec3(const uint8_t* data, std::size_t size, std::size_t base) {
    return simdette::fvec3{sample(data, size, base), sample(data, size, base + 2), sample(data, size, base + 4)};
}

[[nodiscard]] simdette::fmat3 load_mat3(const uint8_t* data, std::size_t size, std::size_t base) {
    return simdette::fmat3{
        sample(data, size, base + 0),  sample(data, size, base + 2),  sample(data, size, base + 4),
        sample(data, size, base + 6),  sample(data, size, base + 8),  sample(data, size, base + 10),
        sample(data, size, base + 12), sample(data, size, base + 14), sample(data, size, base + 16),
    };
}

[[nodiscard]] AzmaStatus fuzz_target(void*, const uint8_t* data, std::size_t size) {
    const simdette::fvec3 v = load_vec3(data, size, 0);
    const simdette::fvec3 w = load_vec3(data, size, 9);

    const float l2 = simdette::length_squared(v);
    const float dd = simdette::dot(v, v);

    if (l2 < -1.0e-4F || !approx(l2, dd, 1.0e-3F)) {
        return AZMA_STATUS_ERROR;
    }

    const simdette::fvec3 vn = simdette::normalize(v);
    if (simdette::length(v) > 1.0e-4F) {
        if (!approx(simdette::length(vn), 1.0F, 2.0e-3F)) {
            return AZMA_STATUS_ERROR;
        }
    }

    const simdette::fmat3 m = load_mat3(data, size, 21);
    const simdette::fmat3 tt = simdette::transpose(simdette::transpose(m));
    for (std::size_t r = 0; r < 3; ++r) {
        for (std::size_t c = 0; c < 3; ++c) {
            if (!approx(tt(r, c), m(r, c), 1.0e-4F)) {
                return AZMA_STATUS_ERROR;
            }
        }
    }

    const simdette::fmat3 id = simdette::fmat3::identity();
    const simdette::fvec3 idv = id * w;
    if (!approx(idv.x, w.x, 1.0e-5F) || !approx(idv.y, w.y, 1.0e-5F) || !approx(idv.z, w.z, 1.0e-5F)) {
        return AZMA_STATUS_ERROR;
    }

    const float det = simdette::determinant(m);
    if (finite(det) && simdette::abs(det) > 5.0e-3F) {
        const simdette::fmat3 inv = simdette::inverse(m);
        const simdette::fmat3 maybe_i = inv * m;
        if (!approx(maybe_i(0, 0), 1.0F, 8.0e-2F) || !approx(maybe_i(1, 1), 1.0F, 8.0e-2F)
            || !approx(maybe_i(2, 2), 1.0F, 8.0e-2F)) {
            return AZMA_STATUS_ERROR;
        }
    }

    return AZMA_STATUS_OK;
}

} // namespace

int main() {
    const std::array<AzmaFuzzInput, 4> corpus = {
        azma_fuzz_input_from_cstr(""),
        azma_fuzz_input_from_cstr("vector"),
        azma_fuzz_input_from_cstr("matrix-invariant"),
        azma_fuzz_input_from_cstr("\x00\x01\x02\x7F\xFF"),
    };

    AzmaFuzzOptions opt = azma_fuzz_options_default();
    opt.iterations = 5000;
    opt.max_input_size = 128;
    opt.stop_on_failure = 1;

    AzmaFuzzStats stats{};
    const AzmaStatus st = azma_fuzz_run(&fuzz_target, nullptr, corpus.data(), corpus.size(), &opt, &stats);

    azma_fuzz_print_stats(stdout, &stats);

    return (st == AZMA_STATUS_OK) ? 0 : 1;
}
