#pragma once

#include "simdette/core/batch.hpp"

#include <cmath>
#include <concepts>
#include <cstddef>

namespace simdette {

namespace detail {

template <std::floating_point T, typename Arch>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE batch<T, Arch> map_unary_fp(batch<T, Arch> const& x, auto&& fn) noexcept {
    alignas(alignof(batch<T, Arch>)) T in[batch<T, Arch>::width];
    alignas(alignof(batch<T, Arch>)) T out[batch<T, Arch>::width];

    x.store_unaligned(in);
    for (std::size_t i = 0; i < batch<T, Arch>::width; ++i) {
        out[i] = static_cast<T>(fn(in[i]));
    }

    return batch<T, Arch>::load_unaligned(out);
}

template <std::floating_point T, typename Arch>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE batch<T, Arch> map_binary_fp(batch<T, Arch> const& a,
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

// Scalar transcendental wrappers

template <std::floating_point T>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE T exp(T x) noexcept {
    return std::exp(x);
}

template <std::floating_point T>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE T exp2(T x) noexcept {
    return std::exp2(x);
}

template <std::floating_point T>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE T log(T x) noexcept {
    return std::log(x);
}

template <std::floating_point T>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE T log2(T x) noexcept {
    return std::log2(x);
}

template <std::floating_point T>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE T log10(T x) noexcept {
    return std::log10(x);
}

template <std::floating_point T>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE T pow(T x, T y) noexcept {
    return std::pow(x, y);
}

template <std::floating_point T>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE T sin(T x) noexcept {
    return std::sin(x);
}

template <std::floating_point T>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE T cos(T x) noexcept {
    return std::cos(x);
}

template <std::floating_point T>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE T tan(T x) noexcept {
    return std::tan(x);
}

template <std::floating_point T>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE T asin(T x) noexcept {
    return std::asin(x);
}

template <std::floating_point T>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE T acos(T x) noexcept {
    return std::acos(x);
}

template <std::floating_point T>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE T atan(T x) noexcept {
    return std::atan(x);
}

template <std::floating_point T>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE T atan2(T y, T x) noexcept {
    return std::atan2(y, x);
}

template <std::floating_point T>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE T sinh(T x) noexcept {
    return std::sinh(x);
}

template <std::floating_point T>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE T cosh(T x) noexcept {
    return std::cosh(x);
}

template <std::floating_point T>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE T tanh(T x) noexcept {
    return std::tanh(x);
}

// Batch transcendental wrappers

template <std::floating_point T, typename Arch>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE batch<T, Arch> exp(batch<T, Arch> const& x) noexcept {
    return detail::map_unary_fp<T, Arch>(x, [](T v) noexcept { return simdette::exp(v); });
}

template <std::floating_point T, typename Arch>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE batch<T, Arch> exp2(batch<T, Arch> const& x) noexcept {
    return detail::map_unary_fp<T, Arch>(x, [](T v) noexcept { return simdette::exp2(v); });
}

template <std::floating_point T, typename Arch>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE batch<T, Arch> log(batch<T, Arch> const& x) noexcept {
    return detail::map_unary_fp<T, Arch>(x, [](T v) noexcept { return simdette::log(v); });
}

template <std::floating_point T, typename Arch>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE batch<T, Arch> log2(batch<T, Arch> const& x) noexcept {
    return detail::map_unary_fp<T, Arch>(x, [](T v) noexcept { return simdette::log2(v); });
}

template <std::floating_point T, typename Arch>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE batch<T, Arch> log10(batch<T, Arch> const& x) noexcept {
    return detail::map_unary_fp<T, Arch>(x, [](T v) noexcept { return simdette::log10(v); });
}

template <std::floating_point T, typename Arch>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE batch<T, Arch> pow(batch<T, Arch> const& x,
                                                  batch<T, Arch> const& y) noexcept {
    return detail::map_binary_fp<T, Arch>(x, y, [](T lhs, T rhs) noexcept { return simdette::pow(lhs, rhs); });
}

template <std::floating_point T, typename Arch>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE batch<T, Arch> sin(batch<T, Arch> const& x) noexcept {
    return detail::map_unary_fp<T, Arch>(x, [](T v) noexcept { return simdette::sin(v); });
}

template <std::floating_point T, typename Arch>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE batch<T, Arch> cos(batch<T, Arch> const& x) noexcept {
    return detail::map_unary_fp<T, Arch>(x, [](T v) noexcept { return simdette::cos(v); });
}

template <std::floating_point T, typename Arch>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE batch<T, Arch> tan(batch<T, Arch> const& x) noexcept {
    return detail::map_unary_fp<T, Arch>(x, [](T v) noexcept { return simdette::tan(v); });
}

template <std::floating_point T, typename Arch>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE batch<T, Arch> asin(batch<T, Arch> const& x) noexcept {
    return detail::map_unary_fp<T, Arch>(x, [](T v) noexcept { return simdette::asin(v); });
}

template <std::floating_point T, typename Arch>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE batch<T, Arch> acos(batch<T, Arch> const& x) noexcept {
    return detail::map_unary_fp<T, Arch>(x, [](T v) noexcept { return simdette::acos(v); });
}

template <std::floating_point T, typename Arch>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE batch<T, Arch> atan(batch<T, Arch> const& x) noexcept {
    return detail::map_unary_fp<T, Arch>(x, [](T v) noexcept { return simdette::atan(v); });
}

template <std::floating_point T, typename Arch>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE batch<T, Arch> atan2(batch<T, Arch> const& y,
                                                    batch<T, Arch> const& x) noexcept {
    return detail::map_binary_fp<T, Arch>(y, x, [](T lhs, T rhs) noexcept { return simdette::atan2(lhs, rhs); });
}

template <std::floating_point T, typename Arch>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE batch<T, Arch> sinh(batch<T, Arch> const& x) noexcept {
    return detail::map_unary_fp<T, Arch>(x, [](T v) noexcept { return simdette::sinh(v); });
}

template <std::floating_point T, typename Arch>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE batch<T, Arch> cosh(batch<T, Arch> const& x) noexcept {
    return detail::map_unary_fp<T, Arch>(x, [](T v) noexcept { return simdette::cosh(v); });
}

template <std::floating_point T, typename Arch>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE batch<T, Arch> tanh(batch<T, Arch> const& x) noexcept {
    return detail::map_unary_fp<T, Arch>(x, [](T v) noexcept { return simdette::tanh(v); });
}

} // namespace simdette
