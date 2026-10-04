#pragma once

// ============================================================================
// Scalar backend specialization
// ============================================================================

#include "simdette/core/batch.hpp"
#include "simdette/core/traits.hpp"

namespace simdette {

namespace detail {

template <typename T>
struct batch_width<T, scalar> : std::integral_constant<std::size_t, 1> {};

} // namespace detail

template <typename T>
struct batch<T, scalar> {
    T value{};

    SIMDETTE_ALWAYS_INLINE batch() noexcept = default;

    SIMDETTE_ALWAYS_INLINE explicit batch(T v) noexcept : value(v) {}

    SIMDETTE_ALWAYS_INLINE explicit batch(T const* ptr) noexcept : value(ptr[0]) {}

    [[nodiscard]]
    static SIMDETTE_ALWAYS_INLINE batch load_aligned(T const* ptr) noexcept {
        return batch(ptr);
    }

    [[nodiscard]]
    static SIMDETTE_ALWAYS_INLINE batch load_unaligned(T const* ptr) noexcept {
        return batch(ptr);
    }

    SIMDETTE_ALWAYS_INLINE void store_aligned(T* ptr) noexcept const {
        ptr[0] = value;
    }

    SIMDETTE_ALWAYS_INLINE void store_unaligned(T* ptr) noexcept const {
        ptr[0] = value;
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE T extract(std::size_t index) const noexcept {
        (void)index;
        return value;
    }

    SIMDETTE_ALWAYS_INLINE void to_array(T* ptr) const noexcept {
        ptr[0] = value;
    }

    // Arithmetic
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator+(batch const& o) const noexcept {
        return batch(value + o.value);
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator-(batch const& o) const noexcept {
        return batch(value - o.value);
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator*(batch const& o) const noexcept {
        return batch(value * o.value);
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator/(batch const& o) const noexcept {
        return batch(value / o.value);
    }

    SIMDETTE_ALWAYS_INLINE batch& operator+=(batch const& o) noexcept {
        value += o.value;
        return *this;
    }
    SIMDETTE_ALWAYS_INLINE batch& operator-=(batch const& o) noexcept {
        value -= o.value;
        return *this;
    }
    SIMDETTE_ALWAYS_INLINE batch& operator*=(batch const& o) noexcept {
        value *= o.value;
        return *this;
    }
    SIMDETTE_ALWAYS_INLINE batch& operator/=(batch const& o) noexcept {
        value /= o.value;
        return *this;
    }

    // Comparisons
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator<(batch const& o) const noexcept {
        return batch(value < o.value ? T(1) : T(0));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator<=(batch const& o) const noexcept {
        return batch(value <= o.value ? T(1) : T(0));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator>(batch const& o) const noexcept {
        return batch(value > o.value ? T(1) : T(0));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator>=(batch const& o) const noexcept {
        return batch(value >= o.value ? T(1) : T(0));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator==(batch const& o) const noexcept {
        return batch(value == o.value ? T(1) : T(0));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator!=(batch const& o) const noexcept {
        return batch(value != o.value ? T(1) : T(0));
    }

    // Bitwise (integral only)
    template <typename U = T>
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE std::enable_if_t<std::is_integral_v<U>, batch> operator&(batch const& o) const noexcept {
        return batch(value & o.value);
    }
    template <typename U = T>
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE std::enable_if_t<std::is_integral_v<U>, batch> operator|(batch const& o) const noexcept {
        return batch(value | o.value);
    }
    template <typename U = T>
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE std::enable_if_t<std::is_integral_v<U>, batch> operator^(batch const& o) const noexcept {
        return batch(value ^ o.value);
    }
    template <typename U = T>
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE std::enable_if_t<std::is_integral_v<U>, batch> operator~() const noexcept {
        return batch(~value);
    }

    SIMDETTE_ALWAYS_INLINE batch& operator&=(batch const& o) noexcept {
        value &= o.value;
        return *this;
    }
    SIMDETTE_ALWAYS_INLINE batch& operator|=(batch const& o) noexcept {
        value |= o.value;
        return *this;
    }
    SIMDETTE_ALWAYS_INLINE batch& operator^=(batch const& o) noexcept {
        value ^= o.value;
        return *this;
    }

    // Logical
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator&&(batch const& o) const noexcept {
        return batch(value && o.value ? T(1) : T(0));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator||(batch const& o) const noexcept {
        return batch(value || o.value ? T(1) : T(0));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator!() const noexcept {
        return batch(value ? T(0) : T(1));
    }

    // Unary
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator+() const noexcept { return *this; }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator-() const noexcept { return batch(-value); }

    // Reductions
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE T reduce_add() const noexcept { return value; }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE T reduce_min() const noexcept { return value; }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE T reduce_max() const noexcept { return value; }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE bool all() const noexcept { return value != T(0); }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE bool any() const noexcept { return value != T(0); }
};

} // namespace simdette
