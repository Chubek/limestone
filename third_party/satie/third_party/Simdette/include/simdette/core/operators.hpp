#pragma once

#include "simdette/core/batch.hpp"
#include "simdette/core/traits.hpp"

namespace simdette {

// ============================================================================
// Non-member arithmetic operators
// ============================================================================

/**
 * @brief Scalar * batch
 */
template <typename T, typename Arch>
[[nodiscard]]
SIMDETTE_ALWAYS_INLINE batch<T, Arch> operator*(T scalar, batch<T, Arch> const& vec) noexcept {
    return vec * scalar;
}

/**
 * @brief batch * scalar
 */
template <typename T, typename Arch>
[[nodiscard]]
SIMDETTE_ALWAYS_INLINE batch<T, Arch> operator*(batch<T, Arch> const& vec, T scalar) noexcept {
    batch<T, Arch> s(scalar);
    return vec * s;
}

/**
 * @brief Scalar / batch (element-wise)
 */
template <typename T, typename Arch>
[[nodiscard]]
SIMDETTE_ALWAYS_INLINE batch<T, Arch> operator/(T scalar, batch<T, Arch> const& vec) noexcept {
    batch<T, Arch> s(scalar);
    return s / vec;
}

/**
 * @brief batch / scalar (element-wise)
 */
template <typename T, typename Arch>
[[nodiscard]]
SIMDETTE_ALWAYS_INLINE batch<T, Arch> operator/(batch<T, Arch> const& vec, T scalar) noexcept {
    batch<T, Arch> s(scalar);
    return vec / s;
}

/**
 * @brief Scalar + batch
 */
template <typename T, typename Arch>
[[nodiscard]]
SIMDETTE_ALWAYS_INLINE batch<T, Arch> operator+(T scalar, batch<T, Arch> const& vec) noexcept {
    batch<T, Arch> s(scalar);
    return s + vec;
}

/**
 * @brief batch + scalar
 */
template <typename T, typename Arch>
[[nodiscard]]
SIMDETTE_ALWAYS_INLINE batch<T, Arch> operator+(batch<T, Arch> const& vec, T scalar) noexcept {
    batch<T, Arch> s(scalar);
    return vec + s;
}

/**
 * @brief Scalar - batch
 */
template <typename T, typename Arch>
[[nodiscard]]
SIMDETTE_ALWAYS_INLINE batch<T, Arch> operator-(T scalar, batch<T, Arch> const& vec) noexcept {
    batch<T, Arch> s(scalar);
    return s - vec;
}

/**
 * @brief batch - scalar
 */
template <typename T, typename Arch>
[[nodiscard]]
SIMDETTE_ALWAYS_INLINE batch<T, Arch> operator-(batch<T, Arch> const& vec, T scalar) noexcept {
    batch<T, Arch> s(scalar);
    return vec - s;
}

// ============================================================================
// Comparison operators for result types
// ============================================================================

/**
 * @brief Equal comparison (result is mask).
 */
template <typename T, typename Arch>
[[nodiscard]]
SIMDETTE_ALWAYS_INLINE batch<T, Arch> operator==(batch<T, Arch> const& a, batch<T, Arch> const& b) noexcept {
    return a == b;
}

/**
 * @brief Not equal comparison (result is mask).
 */
template <typename T, typename Arch>
[[nodiscard]]
SIMDETTE_ALWAYS_INLINE batch<T, Arch> operator!=(batch<T, Arch> const& a, batch<T, Arch> const& b) noexcept {
    return a != b;
}

/**
 * @brief Less than comparison (result is mask).
 */
template <typename T, typename Arch>
[[nodiscard]]
SIMDETTE_ALWAYS_INLINE batch<T, Arch> operator<(batch<T, Arch> const& a, batch<T, Arch> const& b) noexcept {
    return a < b;
}

/**
 * @brief Less than or equal comparison (result is mask).
 */
template <typename T, typename Arch>
[[nodiscard]]
SIMDETTE_ALWAYS_INLINE batch<T, Arch> operator<=(batch<T, Arch> const& a, batch<T, Arch> const& b) noexcept {
    return a <= b;
}

/**
 * @brief Greater than comparison (result is mask).
 */
template <typename T, typename Arch>
[[nodiscard]]
SIMDETTE_ALWAYS_INLINE batch<T, Arch> operator>(batch<T, Arch> const& a, batch<T, Arch> const& b) noexcept {
    return a > b;
}

/**
 * @brief Greater than or equal comparison (result is mask).
 */
template <typename T, typename Arch>
[[nodiscard]]
SIMDETTE_ALWAYS_INLINE batch<T, Arch> operator>=(batch<T, Arch> const& a, batch<T, Arch> const& b) noexcept {
    return a >= b;
}

// ============================================================================
// Bitwise operators for integer types
// ============================================================================

/**
 * @brief Bitwise AND.
 */
template <typename T, typename Arch>
[[nodiscard]]
SIMDETTE_ALWAYS_INLINE batch<T, Arch> operator&(batch<T, Arch> const& a, batch<T, Arch> const& b) noexcept {
    return a & b;
}

/**
 * @brief Bitwise OR.
 */
template <typename T, typename Arch>
[[nodiscard]]
SIMDETTE_ALWAYS_INLINE batch<T, Arch> operator|(batch<T, Arch> const& a, batch<T, Arch> const& b) noexcept {
    return a | b;
}

/**
 * @brief Bitwise XOR.
 */
template <typename T, typename Arch>
[[nodiscard]]
SIMDETTE_ALWAYS_INLINE batch<T, Arch> operator^(batch<T, Arch> const& a, batch<T, Arch> const& b) noexcept {
    return a ^ b;
}

/**
 * @brief Bitwise NOT.
 */
template <typename T, typename Arch>
[[nodiscard]]
SIMDETTE_ALWAYS_INLINE batch<T, Arch> operator~(batch<T, Arch> const& a) noexcept {
    return ~a;
}

/**
 * @brief Compound bitwise AND.
 */
template <typename T, typename Arch>
SIMDETTE_ALWAYS_INLINE batch<T, Arch>& operator&=(batch<T, Arch>& a, batch<T, Arch> const& b) noexcept {
    return a &= b;
}

/**
 * @brief Compound bitwise OR.
 */
template <typename T, typename Arch>
SIMDETTE_ALWAYS_INLINE batch<T, Arch>& operator|=(batch<T, Arch>& a, batch<T, Arch> const& b) noexcept {
    return a |= b;
}

/**
 * @brief Compound bitwise XOR.
 */
template <typename T, typename Arch>
SIMDETTE_ALWAYS_INLINE batch<T, Arch>& operator^=(batch<T, Arch>& a, batch<T, Arch> const& b) noexcept {
    return a ^= b;
}

// ============================================================================
// Logical operators for boolean types
// ============================================================================

/**
 * @brief Logical AND.
 */
template <typename T, typename Arch>
[[nodiscard]]
SIMDETTE_ALWAYS_INLINE batch<T, Arch> operator&&(batch<T, Arch> const& a, batch<T, Arch> const& b) noexcept {
    return a && b;
}

/**
 * @brief Logical OR.
 */
template <typename T, typename Arch>
[[nodiscard]]
SIMDETTE_ALWAYS_INLINE batch<T, Arch> operator||(batch<T, Arch> const& a, batch<T, Arch> const& b) noexcept {
    return a || b;
}

/**
 * @brief Logical NOT.
 */
template <typename T, typename Arch>
[[nodiscard]]
SIMDETTE_ALWAYS_INLINE batch<T, Arch> operator!(batch<T, Arch> const& a) noexcept {
    return !a;
}

} // namespace simdette
