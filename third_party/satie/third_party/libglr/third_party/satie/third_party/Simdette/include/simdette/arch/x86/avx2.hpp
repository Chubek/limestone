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
struct batch_width<float, avx2> {
    static constexpr std::size_t value = 8;
};

template <>
struct batch_width<double, avx2> {
    static constexpr std::size_t value = 4;
};

template <>
struct batch_width<int32_t, avx2> {
    static constexpr std::size_t value = 8;
};

template <>
struct batch_width<uint32_t, avx2> {
    static constexpr std::size_t value = 8;
};

template <>
struct batch_width<int64_t, avx2> {
    static constexpr std::size_t value = 4;
};

template <>
struct batch_width<uint64_t, avx2> {
    static constexpr std::size_t value = 4;
};

} // namespace detail

template <>
struct batch<float, avx2> {
    using value_type = float;
    using arch_type = avx2;

    static constexpr std::size_t width = detail::batch_width_v<float, avx2>;

    __m256 value;

    SIMDETTE_ALWAYS_INLINE batch() noexcept = default;

    SIMDETTE_ALWAYS_INLINE explicit batch(__m256 v) noexcept : value(v) {}

    SIMDETTE_ALWAYS_INLINE explicit batch(float scalar) noexcept
        : value(_mm256_set1_ps(scalar)) {}

    SIMDETTE_ALWAYS_INLINE explicit batch(float const* ptr) noexcept
        : value(_mm256_loadu_ps(ptr)) {}

    [[nodiscard]]
    static SIMDETTE_ALWAYS_INLINE batch load_aligned(float const* ptr) noexcept {
        return batch(_mm256_load_ps(ptr));
    }

    [[nodiscard]]
    static SIMDETTE_ALWAYS_INLINE batch load_unaligned(float const* ptr) noexcept {
        return batch(_mm256_loadu_ps(ptr));
    }

    SIMDETTE_ALWAYS_INLINE void store_aligned(float* ptr) noexcept const {
        _mm256_store_ps(ptr, value);
    }

    SIMDETTE_ALWAYS_INLINE void store_unaligned(float* ptr) noexcept const {
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
    SIMDETTE_ALWAYS_INLINE batch operator+(batch const& o) const noexcept {
        return batch(_mm256_add_ps(value, o.value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator-(batch const& o) const noexcept {
        return batch(_mm256_sub_ps(value, o.value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator*(batch const& o) const noexcept {
        return batch(_mm256_mul_ps(value, o.value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator/(batch const& o) const noexcept {
        return batch(_mm256_div_ps(value, o.value));
    }

    SIMDETTE_ALWAYS_INLINE batch& operator+=(batch const& o) noexcept {
        value = _mm256_add_ps(value, o.value);
        return *this;
    }
    SIMDETTE_ALWAYS_INLINE batch& operator-=(batch const& o) noexcept {
        value = _mm256_sub_ps(value, o.value);
        return *this;
    }
    SIMDETTE_ALWAYS_INLINE batch& operator*=(batch const& o) noexcept {
        value = _mm256_mul_ps(value, o.value);
        return *this;
    }
    SIMDETTE_ALWAYS_INLINE batch& operator/=(batch const& o) noexcept {
        value = _mm256_div_ps(value, o.value);
        return *this;
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator<(batch const& o) const noexcept {
        return batch(_mm256_cmplt_ps(value, o.value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator<=(batch const& o) const noexcept {
        return batch(_mm256_cmple_ps(value, o.value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator>(batch const& o) const noexcept {
        return batch(_mm256_cmpgt_ps(value, o.value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator>=(batch const& o) const noexcept {
        return batch(_mm256_cmpge_ps(value, o.value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator==(batch const& o) const noexcept {
        return batch(_mm256_cmpeq_ps(value, o.value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator!=(batch const& o) const noexcept {
        return batch(_mm256_cmpneq_ps(value, o.value));
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator&(batch const& o) const noexcept {
        return batch(_mm256_and_ps(value, o.value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator|(batch const& o) const noexcept {
        return batch(_mm256_or_ps(value, o.value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator^(batch const& o) const noexcept {
        return batch(_mm256_xor_ps(value, o.value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator~() const noexcept {
        return batch(_mm256_xor_ps(value, _mm256_castsi256_ps(_mm256_set1_epi32(-1))));
    }

    SIMDETTE_ALWAYS_INLINE batch& operator&=(batch const& o) noexcept {
        value = _mm256_and_ps(value, o.value);
        return *this;
    }
    SIMDETTE_ALWAYS_INLINE batch& operator|=(batch const& o) noexcept {
        value = _mm256_or_ps(value, o.value);
        return *this;
    }
    SIMDETTE_ALWAYS_INLINE batch& operator^=(batch const& o) noexcept {
        value = _mm256_xor_ps(value, o.value);
        return *this;
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator&&(batch const& o) const noexcept {
        return batch(_mm256_and_ps(value, o.value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator||(batch const& o) const noexcept {
        return batch(_mm256_or_ps(value, o.value));
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
    SIMDETTE_ALWAYS_INLINE batch operator+() const noexcept { return *this; }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE float reduce_add() const noexcept {
        alignas(32) std::array<float, width> lanes{};
        _mm256_store_ps(lanes.data(), value);
        return lanes[0] + lanes[1] + lanes[2] + lanes[3] +
               lanes[4] + lanes[5] + lanes[6] + lanes[7];
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE float reduce_min() const noexcept {
        alignas(32) std::array<float, width> lanes{};
        _mm256_store_ps(lanes.data(), value);
        return std::min({lanes[0], lanes[1], lanes[2], lanes[3],
                         lanes[4], lanes[5], lanes[6], lanes[7]});
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE float reduce_max() const noexcept {
        alignas(32) std::array<float, width> lanes{};
        _mm256_store_ps(lanes.data(), value);
        return std::max({lanes[0], lanes[1], lanes[2], lanes[3],
                         lanes[4], lanes[5], lanes[6], lanes[7]});
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
