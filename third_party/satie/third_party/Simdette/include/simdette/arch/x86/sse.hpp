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
struct batch_width<float, simdette::sse> {
    static constexpr std::size_t value = 4;
};

template <>
struct batch_width<double, simdette::sse> {
    static constexpr std::size_t value = 2;
};

} // namespace detail

template <>
struct batch<float, simdette::sse> {
    using value_type = float;
    using arch_type = simdette::sse;

    static constexpr std::size_t width = detail::batch_width_v<float, arch_type>;

    __m128 value;

    SIMDETTE_ALWAYS_INLINE batch() noexcept = default;

    SIMDETTE_ALWAYS_INLINE explicit batch(__m128 native) noexcept
        : value(native) {}

    SIMDETTE_ALWAYS_INLINE explicit batch(float scalar) noexcept
        : value(_mm_set1_ps(scalar)) {}

    SIMDETTE_ALWAYS_INLINE explicit batch(float const* values) noexcept
        : value(_mm_loadu_ps(values)) {}

    [[nodiscard]]
    static SIMDETTE_ALWAYS_INLINE batch load_aligned(float const* ptr) noexcept {
        return batch(_mm_load_ps(ptr));
    }

    [[nodiscard]]
    static SIMDETTE_ALWAYS_INLINE batch load_unaligned(float const* ptr) noexcept {
        return batch(_mm_loadu_ps(ptr));
    }

    SIMDETTE_ALWAYS_INLINE void store_aligned(float* ptr) noexcept {
        _mm_store_ps(ptr, value);
    }

    SIMDETTE_ALWAYS_INLINE void store_unaligned(float* ptr) noexcept {
        _mm_storeu_ps(ptr, value);
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE float extract(std::size_t index) const noexcept {
        alignas(16) std::array<float, width> lanes{};
        _mm_store_ps(lanes.data(), value);
        return lanes[index];
    }

    SIMDETTE_ALWAYS_INLINE void to_array(float* ptr) const noexcept {
        _mm_storeu_ps(ptr, value);
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator+(batch const& other) const noexcept {
        return batch(_mm_add_ps(value, other.value));
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator-(batch const& other) const noexcept {
        return batch(_mm_sub_ps(value, other.value));
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator*(batch const& other) const noexcept {
        return batch(_mm_mul_ps(value, other.value));
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator/(batch const& other) const noexcept {
        return batch(_mm_div_ps(value, other.value));
    }

    SIMDETTE_ALWAYS_INLINE batch& operator+=(batch const& other) noexcept {
        value = _mm_add_ps(value, other.value);
        return *this;
    }

    SIMDETTE_ALWAYS_INLINE batch& operator-=(batch const& other) noexcept {
        value = _mm_sub_ps(value, other.value);
        return *this;
    }

    SIMDETTE_ALWAYS_INLINE batch& operator*=(batch const& other) noexcept {
        value = _mm_mul_ps(value, other.value);
        return *this;
    }

    SIMDETTE_ALWAYS_INLINE batch& operator/=(batch const& other) noexcept {
        value = _mm_div_ps(value, other.value);
        return *this;
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator<(batch const& other) const noexcept {
        return batch(_mm_cmplt_ps(value, other.value));
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator<=(batch const& other) const noexcept {
        return batch(_mm_cmple_ps(value, other.value));
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator>(batch const& other) const noexcept {
        return batch(_mm_cmpgt_ps(value, other.value));
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator>=(batch const& other) const noexcept {
        return batch(_mm_cmpge_ps(value, other.value));
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator==(batch const& other) const noexcept {
        return batch(_mm_cmpeq_ps(value, other.value));
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator!=(batch const& other) const noexcept {
        return batch(_mm_cmpneq_ps(value, other.value));
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator&(batch const& other) const noexcept {
        return batch(_mm_and_ps(value, other.value));
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator|(batch const& other) const noexcept {
        return batch(_mm_or_ps(value, other.value));
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator^(batch const& other) const noexcept {
        return batch(_mm_xor_ps(value, other.value));
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator~() const noexcept {
        return batch(_mm_xor_ps(value, _mm_castsi128_ps(_mm_set1_epi32(-1))));
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator&&(batch const& other) const noexcept {
        return batch(_mm_and_ps(value, other.value));
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator||(batch const& other) const noexcept {
        return batch(_mm_or_ps(value, other.value));
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator!() const noexcept {
        return batch(*this == batch(0.0f));
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator-() const noexcept {
        return batch(_mm_sub_ps(_mm_setzero_ps(), value));
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator+() const noexcept {
        return *this;
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE float reduce_add() const noexcept {
        alignas(16) std::array<float, width> lanes{};
        _mm_store_ps(lanes.data(), value);
        return lanes[0] + lanes[1] + lanes[2] + lanes[3];
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE float reduce_min() const noexcept {
        alignas(16) std::array<float, width> lanes{};
        _mm_store_ps(lanes.data(), value);
        return std::min(std::min(lanes[0], lanes[1]), std::min(lanes[2], lanes[3]));
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE float reduce_max() const noexcept {
        alignas(16) std::array<float, width> lanes{};
        _mm_store_ps(lanes.data(), value);
        return std::max(std::max(lanes[0], lanes[1]), std::max(lanes[2], lanes[3]));
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE bool all() const noexcept {
        return _mm_movemask_ps(value) == 0xF;
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE bool any() const noexcept {
        return _mm_movemask_ps(value) != 0;
    }
};

} // namespace simdette
