#pragma once

#include "simdette/core/batch.hpp"

#include <wasm_simd128.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <type_traits>

namespace simdette {

namespace detail {

template <>
struct batch_width<float, simd128> {
    static constexpr std::size_t value = 4;
};

template <>
struct batch_width<int32_t, simd128> {
    static constexpr std::size_t value = 4;
};

template <>
struct batch_width<uint32_t, simd128> {
    static constexpr std::size_t value = 4;
};

} // namespace detail

template <>
struct batch<float, simd128> {
    using value_type = float;
    using arch_type = simd128;

    static constexpr std::size_t width = detail::batch_width_v<float, simd128>;

    v128_t value;

    SIMDETTE_ALWAYS_INLINE batch() noexcept = default;

    SIMDETTE_ALWAYS_INLINE explicit batch(v128_t v) noexcept : value(v) {}

    SIMDETTE_ALWAYS_INLINE explicit batch(float scalar) noexcept
        : value(v128_load(&scalar)) {}

    SIMDETTE_ALWAYS_INLINE explicit batch(float const* ptr) noexcept
        : value(v128_load(ptr)) {}

    [[nodiscard]]
    static SIMDETTE_ALWAYS_INLINE batch load_aligned(float const* ptr) noexcept {
        return batch(v128_load(ptr));
    }

    [[nodiscard]]
    static SIMDETTE_ALWAYS_INLINE batch load_unaligned(float const* ptr) noexcept {
        return batch(v128_load(ptr));
    }

    SIMDETTE_ALWAYS_INLINE void store_aligned(float* ptr) noexcept const {
        v128_store(ptr, value);
    }

    SIMDETTE_ALWAYS_INLINE void store_unaligned(float* ptr) noexcept const {
        v128_store(ptr, value);
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE float extract(std::size_t index) const noexcept {
        alignas(16) std::array<float, width> lanes{};
        v128_store(lanes.data(), value);
        return lanes[index];
    }

    SIMDETTE_ALWAYS_INLINE void to_array(float* ptr) const noexcept {
        v128_store(ptr, value);
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator+(batch const& o) const noexcept {
        return batch(v128_add_f32x4(value, o.value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator-(batch const& o) const noexcept {
        return batch(v128_sub_f32x4(value, o.value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator*(batch const& o) const noexcept {
        return batch(v128_mul_f32x4(value, o.value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator/(batch const& o) const noexcept {
        return batch(v128_div_f32x4(value, o.value));
    }

    SIMDETTE_ALWAYS_INLINE batch& operator+=(batch const& o) noexcept {
        value = v128_add_f32x4(value, o.value);
        return *this;
    }
    SIMDETTE_ALWAYS_INLINE batch& operator-=(batch const& o) noexcept {
        value = v128_sub_f32x4(value, o.value);
        return *this;
    }
    SIMDETTE_ALWAYS_INLINE batch& operator*=(batch const& o) noexcept {
        value = v128_mul_f32x4(value, o.value);
        return *this;
    }
    SIMDETTE_ALWAYS_INLINE batch& operator/=(batch const& o) noexcept {
        value = v128_div_f32x4(value, o.value);
        return *this;
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator<(batch const& o) const noexcept {
        return batch(v128_lt_f32x4(value, o.value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator<=(batch const& o) const noexcept {
        return batch(v128_le_f32x4(value, o.value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator>(batch const& o) const noexcept {
        return batch(v128_gt_f32x4(value, o.value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator>=(batch const& o) const noexcept {
        return batch(v128_ge_f32x4(value, o.value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator==(batch const& o) const noexcept {
        return batch(v128_eq_f32x4(value, o.value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator!=(batch const& o) const noexcept {
        return batch(v128_ne_f32x4(value, o.value));
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator&(batch const& o) const noexcept {
        return batch(v128_and(value, o.value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator|(batch const& o) const noexcept {
        return batch(v128_or(value, o.value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator^(batch const& o) const noexcept {
        return batch(v128_xor(value, o.value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator~() const noexcept {
        return batch(v128_xor(value, v128_load((uint32x4_t[]){{0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF}})));
    }

    SIMDETTE_ALWAYS_INLINE batch& operator&=(batch const& o) noexcept {
        value = v128_and(value, o.value);
        return *this;
    }
    SIMDETTE_ALWAYS_INLINE batch& operator|=(batch const& o) noexcept {
        value = v128_or(value, o.value);
        return *this;
    }
    SIMDETTE_ALWAYS_INLINE batch& operator^=(batch const& o) noexcept {
        value = v128_xor(value, o.value);
        return *this;
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator&&(batch const& o) const noexcept {
        return batch(v128_and(value, o.value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator||(batch const& o) const noexcept {
        return batch(v128_or(value, o.value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator!() const noexcept {
        return batch(v128_eq_f32x4(value, v128_load(&zero_f32)));
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator-() const noexcept {
        return batch(v128_sub_f32x4(v128_load(&zero_f32), value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator+() const noexcept { return *this; }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE float reduce_add() const noexcept {
        alignas(16) std::array<float, width> lanes{};
        v128_store(lanes.data(), value);
        return lanes[0] + lanes[1] + lanes[2] + lanes[3];
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE float reduce_min() const noexcept {
        alignas(16) std::array<float, width> lanes{};
        v128_store(lanes.data(), value);
        return std::min(std::min(lanes[0], lanes[1]), std::min(lanes[2], lanes[3]));
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE float reduce_max() const noexcept {
        alignas(16) std::array<float, width> lanes{};
        v128_store(lanes.data(), value);
        return std::max(std::max(lanes[0], lanes[1]), std::max(lanes[2], lanes[3]));
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE bool all() const noexcept {
        uint32x4_t mask = v128_load((uint32x4_t[]){{0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF}});
        return v128_eq(value, mask) == v128_load((uint32x4_t[]){{0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF}});
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE bool any() const noexcept {
        return !v128_eq(value, v128_load(&zero_f32));
    }

    static const float zero_f32;
};

const float batch<float, simd128>::zero_f32 = 0.0f;

} // namespace simdette
