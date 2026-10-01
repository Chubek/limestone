#pragma once

#include "simdette/core/batch.hpp"

#include <immintrin.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <type_traits>

namespace simdette {

namespace detail {

template <>
struct batch_width<float, simdette::avx> {
    static constexpr std::size_t value = 8;
};

template <>
struct batch_width<double, simdette::avx> {
    static constexpr std::size_t value = 4;
};

} // namespace detail

template <>
struct batch<float, simdette::avx> {
    using value_type = float;
    using arch_type = simdette::avx;

    static constexpr std::size_t width = detail::batch_width_v<float, arch_type>;

    __m256 value;

    SIMDETTE_ALWAYS_INLINE batch() noexcept = default;

    SIMDETTE_ALWAYS_INLINE explicit batch(__m256 native) noexcept
        : value(native) {}

    SIMDETTE_ALWAYS_INLINE explicit batch(float scalar) noexcept
        : value(_mm256_set1_ps(scalar)) {}

    SIMDETTE_ALWAYS_INLINE explicit batch(float const* values) noexcept
        : value(_mm256_loadu_ps(values)) {}

    [[nodiscard]]
    static SIMDETTE_ALWAYS_INLINE batch load_aligned(float const* ptr) noexcept {
        return batch(_mm256_load_ps(ptr));
    }

    [[nodiscard]]
    static SIMDETTE_ALWAYS_INLINE batch load_unaligned(float const* ptr) noexcept {
        return batch(_mm256_loadu_ps(ptr));
    }

    SIMDETTE_ALWAYS_INLINE void store_aligned(float* ptr) noexcept {
        _mm256_store_ps(ptr, value);
    }

    SIMDETTE_ALWAYS_INLINE void store_unaligned(float* ptr) noexcept {
        _mm256_storeu_ps(ptr, value);
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE float extract(std::size_t index) const noexcept {
        alignas(32) std::array<float, width> lanes{};
        _mm256_store_ps(lanes.data(), value);
        return lanes[index];
    }

    SIMDETTE_ALWAYS_INLINE void to_array(float* ptr) const noexcept {
        _mm256_storeu_ps(ptr, value);
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator+(batch const& other) const noexcept {
        return batch(_mm256_add_ps(value, other.value));
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator-(batch const& other) const noexcept {
        return batch(_mm256_sub_ps(value, other.value));
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator*(batch const& other) const noexcept {
        return batch(_mm256_mul_ps(value, other.value));
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator/(batch const& other) const noexcept {
        return batch(_mm256_div_ps(value, other.value));
    }

    SIMDETTE_ALWAYS_INLINE batch& operator+=(batch const& other) noexcept {
        value = _mm256_add_ps(value, other.value);
        return *this;
    }

    SIMDETTE_ALWAYS_INLINE batch& operator-=(batch const& other) noexcept {
        value = _mm256_sub_ps(value, other.value);
        return *this;
    }

    SIMDETTE_ALWAYS_INLINE batch& operator*=(batch const& other) noexcept {
        value = _mm256_mul_ps(value, other.value);
        return *this;
    }

    SIMDETTE_ALWAYS_INLINE batch& operator/=(batch const& other) noexcept {
        value = _mm256_div_ps(value, other.value);
        return *this;
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator<(batch const& other) const noexcept {
        return batch(_mm256_cmp_ps(value, other.value, _CMP_LT_OQ));
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator<=(batch const& other) const noexcept {
        return batch(_mm256_cmp_ps(value, other.value, _CMP_LE_OQ));
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator>(batch const& other) const noexcept {
        return batch(_mm256_cmp_ps(value, other.value, _CMP_GT_OQ));
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator>=(batch const& other) const noexcept {
        return batch(_mm256_cmp_ps(value, other.value, _CMP_GE_OQ));
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator==(batch const& other) const noexcept {
        return batch(_mm256_cmp_ps(value, other.value, _CMP_EQ_OQ));
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator!=(batch const& other) const noexcept {
        return batch(_mm256_cmp_ps(value, other.value, _CMP_NEQ_OQ));
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator&(batch const& other) const noexcept {
        return batch(_mm256_and_ps(value, other.value));
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator|(batch const& other) const noexcept {
        return batch(_mm256_or_ps(value, other.value));
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator^(batch const& other) const noexcept {
        return batch(_mm256_xor_ps(value, other.value));
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator~() const noexcept {
        return batch(_mm256_xor_ps(value, _mm256_castsi256_ps(_mm256_set1_epi32(-1))));
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator&&(batch const& other) const noexcept {
        return batch(_mm256_and_ps(value, other.value));
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator||(batch const& other) const noexcept {
        return batch(_mm256_or_ps(value, other.value));
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator!() const noexcept {
        return batch(*this == batch(0.0f));
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator-() const noexcept {
        return batch(_mm256_sub_ps(_mm256_setzero_ps(), value));
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator+() const noexcept {
        return *this;
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE float reduce_add() const noexcept {
        alignas(32) std::array<float, width> lanes{};
        _mm256_store_ps(lanes.data(), value);
        float sum = 0.0f;
        for (float lane : lanes) {
            sum += lane;
        }
        return sum;
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE float reduce_min() const noexcept {
        alignas(32) std::array<float, width> lanes{};
        _mm256_store_ps(lanes.data(), value);
        float result = lanes[0];
        for (std::size_t i = 1; i < width; ++i) {
            result = std::min(result, lanes[i]);
        }
        return result;
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE float reduce_max() const noexcept {
        alignas(32) std::array<float, width> lanes{};
        _mm256_store_ps(lanes.data(), value);
        float result = lanes[0];
        for (std::size_t i = 1; i < width; ++i) {
            result = std::max(result, lanes[i]);
        }
        return result;
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE bool all() const noexcept {
        return _mm256_movemask_ps(value) == 0xFF;
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE bool any() const noexcept {
        return _mm256_movemask_ps(value) != 0;
    }
};

} // namespace simdette
