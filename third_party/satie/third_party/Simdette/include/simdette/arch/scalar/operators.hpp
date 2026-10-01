#pragma once

#include "simdette/arch/scalar/register.hpp"

namespace simdette {

// ============================================================================
// Scalar backend free operators
// ============================================================================

template <typename T>
[[nodiscard]]
SIMDETTE_ALWAYS_INLINE batch<T, scalar> operator+(T lhs, batch<T, scalar> const& rhs) noexcept {
    return batch<T, scalar>(lhs) + rhs;
}

template <typename T>
[[nodiscard]]
SIMDETTE_ALWAYS_INLINE batch<T, scalar> operator+(batch<T, scalar> const& lhs, T rhs) noexcept {
    return lhs + batch<T, scalar>(rhs);
}

template <typename T>
[[nodiscard]]
SIMDETTE_ALWAYS_INLINE batch<T, scalar> operator-(T lhs, batch<T, scalar> const& rhs) noexcept {
    return batch<T, scalar>(lhs) - rhs;
}

template <typename T>
[[nodiscard]]
SIMDETTE_ALWAYS_INLINE batch<T, scalar> operator-(batch<T, scalar> const& lhs, T rhs) noexcept {
    return lhs - batch<T, scalar>(rhs);
}

template <typename T>
[[nodiscard]]
SIMDETTE_ALWAYS_INLINE batch<T, scalar> operator*(T lhs, batch<T, scalar> const& rhs) noexcept {
    return batch<T, scalar>(lhs) * rhs;
}

template <typename T>
[[nodiscard]]
SIMDETTE_ALWAYS_INLINE batch<T, scalar> operator*(batch<T, scalar> const& lhs, T rhs) noexcept {
    return lhs * batch<T, scalar>(rhs);
}

template <typename T>
[[nodiscard]]
SIMDETTE_ALWAYS_INLINE batch<T, scalar> operator/(T lhs, batch<T, scalar> const& rhs) noexcept {
    return batch<T, scalar>(lhs) / rhs;
}

template <typename T>
[[nodiscard]]
SIMDETTE_ALWAYS_INLINE batch<T, scalar> operator/(batch<T, scalar> const& lhs, T rhs) noexcept {
    return lhs / batch<T, scalar>(rhs);
}

} // namespace simdette
