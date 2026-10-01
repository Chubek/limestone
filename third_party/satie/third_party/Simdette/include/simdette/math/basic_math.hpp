#pragma once

#include "simdette/core/batch.hpp"

#include <cmath>
#include <concepts>
#include <cstddef>
#include <limits>
#include <type_traits>

namespace simdette {

namespace detail {

template <typename T>
concept arithmetic_scalar = std::is_arithmetic_v<T>;

template <typename T, typename Arch>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE batch<T, Arch> map_unary(batch<T, Arch> const& x, auto&& fn) noexcept {
    alignas(alignof(batch<T, Arch>)) T in[batch<T, Arch>::width];
    alignas(alignof(batch<T, Arch>)) T out[batch<T, Arch>::width];

    x.store_unaligned(in);
    for (std::size_t i = 0; i < batch<T, Arch>::width; ++i) {
        out[i] = static_cast<T>(fn(in[i]));
    }
    return batch<T, Arch>::load_unaligned(out);
}

template <typename T, typename Arch>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE batch<T, Arch> map_binary(batch<T, Arch> const& a,
                                                         batch<T, Arch> const& b,
                                                         auto&& fn) noexcept {
    alignas(alignof(batch<T, Arch>)) T lhs[batch<T, Arch>::width];
    alignas(alignof(batch<T, Arch>)) T rhs[batch<T, Arch>::width];
    alignas(alignof(batch<T, Arch>)) T out[batch<T, Arch>::width];

    a.store_unaligned(lhs);
    b.store_unaligned(rhs);
    for (std::size_t i = 0; i < batch<T, Arch>::width; ++i) {
        out[i] = static_cast<T>(fn(lhs[i], rhs[i]));
    }
    return batch<T, Arch>::load_unaligned(out);
}

} // namespace detail

// Scalar wrappers

template <detail::arithmetic_scalar T>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE T abs(T x) noexcept {
    if constexpr (std::is_unsigned_v<T>) {
        return x;
    } else {
        using std::abs;
        return static_cast<T>(abs(x));
    }
}

template <std::floating_point T>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE T sqrt(T x) noexcept {
    return std::sqrt(x);
}

template <std::floating_point T>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE T rsqrt(T x) noexcept {
    return static_cast<T>(1) / std::sqrt(x);
}

template <std::floating_point T>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE T floor(T x) noexcept {
    return std::floor(x);
}

template <std::floating_point T>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE T ceil(T x) noexcept {
    return std::ceil(x);
}

template <std::floating_point T>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE T round(T x) noexcept {
    return std::round(x);
}

template <std::floating_point T>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE T trunc(T x) noexcept {
    return std::trunc(x);
}

template <typename T>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE T min(T a, T b) noexcept {
    return (a < b) ? a : b;
}

template <typename T>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE T max(T a, T b) noexcept {
    return (a > b) ? a : b;
}

template <typename T>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE T clamp(T x, T lo, T hi) noexcept {
    return min(max(x, lo), hi);
}

template <std::floating_point T>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE bool is_nan(T x) noexcept {
    return std::isnan(x);
}

template <std::floating_point T>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE bool is_inf(T x) noexcept {
    return std::isinf(x);
}

// Batch wrappers

template <typename T, typename Arch>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE batch<T, Arch> abs(batch<T, Arch> const& x) noexcept {
    if constexpr (std::is_unsigned_v<T>) {
        return x;
    } else {
        return detail::map_unary<T, Arch>(x, [](T v) noexcept { return simdette::abs(v); });
    }
}

template <std::floating_point T, typename Arch>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE batch<T, Arch> sqrt(batch<T, Arch> const& x) noexcept {
    return detail::map_unary<T, Arch>(x, [](T v) noexcept { return simdette::sqrt(v); });
}

template <std::floating_point T, typename Arch>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE batch<T, Arch> rsqrt(batch<T, Arch> const& x) noexcept {
    return detail::map_unary<T, Arch>(x, [](T v) noexcept { return simdette::rsqrt(v); });
}

template <std::floating_point T, typename Arch>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE batch<T, Arch> floor(batch<T, Arch> const& x) noexcept {
    return detail::map_unary<T, Arch>(x, [](T v) noexcept { return simdette::floor(v); });
}

template <std::floating_point T, typename Arch>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE batch<T, Arch> ceil(batch<T, Arch> const& x) noexcept {
    return detail::map_unary<T, Arch>(x, [](T v) noexcept { return simdette::ceil(v); });
}

template <std::floating_point T, typename Arch>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE batch<T, Arch> round(batch<T, Arch> const& x) noexcept {
    return detail::map_unary<T, Arch>(x, [](T v) noexcept { return simdette::round(v); });
}

template <std::floating_point T, typename Arch>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE batch<T, Arch> trunc(batch<T, Arch> const& x) noexcept {
    return detail::map_unary<T, Arch>(x, [](T v) noexcept { return simdette::trunc(v); });
}

template <typename T, typename Arch>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE batch<T, Arch> min(batch<T, Arch> const& a,
                                                  batch<T, Arch> const& b) noexcept {
    return detail::map_binary<T, Arch>(a, b, [](T lhs, T rhs) noexcept { return simdette::min(lhs, rhs); });
}

template <typename T, typename Arch>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE batch<T, Arch> max(batch<T, Arch> const& a,
                                                  batch<T, Arch> const& b) noexcept {
    return detail::map_binary<T, Arch>(a, b, [](T lhs, T rhs) noexcept { return simdette::max(lhs, rhs); });
}

template <typename T, typename Arch>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE batch<T, Arch> clamp(batch<T, Arch> const& x,
                                                    batch<T, Arch> const& lo,
                                                    batch<T, Arch> const& hi) noexcept {
    return simdette::min(simdette::max(x, lo), hi);
}

} // namespace simdette
