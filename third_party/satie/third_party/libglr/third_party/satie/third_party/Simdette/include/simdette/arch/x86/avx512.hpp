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
struct batch_width<float, avx512> {
    static constexpr std::size_t value = 16;
};

template <>
struct batch_width<double, avx512> {
    static constexpr std::size_t value = 8;
};

template <>
struct batch_width<int32_t, avx512> {
    static constexpr std::size_t value = 16;
};

template <>
struct batch_width<uint32_t, avx512> {
    static constexpr std::size_t value = 16;
};

template <>
struct batch_width<int64_t, avx512> {
    static constexpr std::size_t value = 8;
};

template <>
struct batch_width<uint64_t, avx512> {
    static constexpr std::size_t value = 8;
};

} // namespace detail

template <>
struct batch<float, avx512> {
    using value_type = float;
    using arch_type = avx512;

    static constexpr std::size_t width = detail::batch_width_v<float, avx512>;

    __m512 value;

    SIMDETTE_ALWAYS_INLINE batch() noexcept = default;

    SIMDETTE_ALWAYS_INLINE explicit batch(__m512 v) noexcept : value(v) {}

    SIMDETTE_ALWAYS_INLINE explicit batch(float scalar) noexcept
        : value(_mm512_set1_ps(scalar)) {}

    SIMDETTE_ALWAYS_INLINE explicit batch(float const* ptr) noexcept
        : value(_mm512_loadu_ps(ptr)) {}

    [[nodiscard]]
    static SIMDETTE_ALWAYS_INLINE batch load_aligned(float const* ptr) noexcept {
        return batch(_mm512_load_ps(ptr));
    }

    [[nodiscard]]
    static SIMDETTE_ALWAYS_INLINE batch load_unaligned(float const* ptr) noexcept {
        return batch(_mm512_loadu_ps(ptr));
    }

    SIMDETTE_ALWAYS_INLINE void store_aligned(float* ptr) noexcept const {
        _mm512_store_ps(ptr, value);
    }

    SIMDETTE_ALWAYS_INLINE void store_unaligned(float* ptr) noexcept const {
        _mm512_storeu_ps(ptr, value);
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE float extract(std::size_t index) const noexcept {
        alignas(64) std::array<float, width> lanes{};
        _mm512_store_ps(lanes.data(), value);
        return lanes[index];
    }

    SIMDETTE_ALWAYS_INLINE void to_array(float* ptr) const noexcept {
        _mm512_storeu_ps(ptr, value);
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator+(batch const& o) const noexcept {
        return batch(_mm512_add_ps(value, o.value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator-(batch const& o) const noexcept {
        return batch(_mm512_sub_ps(value, o.value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator*(batch const& o) const noexcept {
        return batch(_mm512_mul_ps(value, o.value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator/(batch const& o) const noexcept {
        return batch(_mm512_div_ps(value, o.value));
    }

    SIMDETTE_ALWAYS_INLINE batch& operator+=(batch const& o) noexcept {
        value = _mm512_add_ps(value, o.value);
        return *this;
    }
    SIMDETTE_ALWAYS_INLINE batch& operator-=(batch const& o) noexcept {
        value = _mm512_sub_ps(value, o.value);
        return *this;
    }
    SIMDETTE_ALWAYS_INLINE batch& operator*=(batch const& o) noexcept {
        value = _mm512_mul_ps(value, o.value);
        return *this;
    }
    SIMDETTE_ALWAYS_INLINE batch& operator/=(batch const& o) noexcept {
        value = _mm512_div_ps(value, o.value);
        return *this;
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator<(batch const& o) const noexcept {
        return batch(_mm512_cmplt_ps_mask(value, o.value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator<=(batch const& o) const noexcept {
        return batch(_mm512_cmpnle_ps_mask(value, o.value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator>(batch const& o) const noexcept {
        return batch(_mm512_cmpgt_ps_mask(value, o.value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator>=(batch const& o) const noexcept {
        return batch(_mm512_cmpnge_ps_mask(value, o.value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator==(batch const& o) const noexcept {
        return batch(_mm512_cmpeq_ps_mask(value, o.value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator!=(batch const& o) const noexcept {
        return batch(_mm512_cmpneq_ps_mask(value, o.value));
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator&(batch const& o) const noexcept {
        return batch(_mm512_and_ps(value, o.value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator|(batch const& o) const noexcept {
        return batch(_mm512_or_ps(value, o.value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator^(batch const& o) const noexcept {
        return batch(_mm512_xor_ps(value, o.value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator~() const noexcept {
        return batch(_mm512_xor_ps(value, _mm512_castsi512_ps(_mm512_set1_epi32(-1))));
    }

    SIMDETTE_ALWAYS_INLINE batch& operator&=(batch const& o) noexcept {
        value = _mm512_and_ps(value, o.value);
        return *this;
    }
    SIMDETTE_ALWAYS_INLINE batch& operator|=(batch const& o) noexcept {
        value = _mm512_or_ps(value, o.value);
        return *this;
    }
    SIMDETTE_ALWAYS_INLINE batch& operator^=(batch const& o) noexcept {
        value = _mm512_xor_ps(value, o.value);
        return *this;
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator&&(batch const& o) const noexcept {
        return batch(_mm512_and_ps(value, o.value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator||(batch const& o) const noexcept {
        return batch(_mm512_or_ps(value, o.value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator!() const noexcept {
        return batch(*this == batch(0.0f));
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator-() const noexcept {
        return batch(_mm512_sub_ps(_mm512_setzero_ps(), value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator+() const noexcept { return *this; }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE float reduce_add() const noexcept {
        alignas(64) std::array<float, width> lanes{};
        _mm512_store_ps(lanes.data(), value);
        float sum = 0;
        for (size_t i = 0; i < width; ++i) sum += lanes[i];
        return sum;
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE float reduce_min() const noexcept {
        alignas(64) std::array<float, width> lanes{};
        _mm512_store_ps(lanes.data(), value);
        float mn = lanes[0];
        for (size_t i = 1; i < width; ++i) mn = std::min(mn, lanes[i]);
        return mn;
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE float reduce_max() const noexcept {
        alignas(64) std::array<float, width> lanes{};
        _mm512_store_ps(lanes.data(), value);
        float mx = lanes[0];
        for (size_t i = 1; i < width; ++i) mx = std::max(mx, lanes[i]);
        return mx;
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE bool all() const noexcept {
        return _mm512_testz_ps(value, value) == 0;
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE bool any() const noexcept {
        return _mm512_testz_ps(value, value) != 0;
    }
};

} // namespace simdette
