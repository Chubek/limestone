#pragma once

#include "simdette/setup/config.hpp"
#include "simdette/setup/detect_arch.hpp"
#include "simdette/core/traits.hpp"

#include <type_traits>
#include <cstdint>
#include <cstdint>

namespace simdette {

/**
 * @brief Primary template for SIMD batch operations.
 * 
 * This is the generic frontend interface that will be specialized
 * for specific architectures in the arch/ directory.
 * 
 * @tparam T The data type (float, double, int32_t, etc.)
 * @tparam Arch The architecture tag (avx2, neon, scalar, etc.)
 */
template <typename T, typename Arch = simdette::default_arch>
struct batch;

// ============================================================================
// Forward declarations for architecture-specific specializations
// ============================================================================

// Scalar backend (fallback)
template <typename T>
struct batch<T, simdette::scalar>;

// x86 backends
template <typename T>
struct batch<T, simdette::sse>;
template <typename T>
struct batch<T, simdette::avx>;
template <typename T>
struct batch<T, simdette::avx2>;
template <typename T>
struct batch<T, simdette::avx512>;

// ARM backends
template <typename T>
struct batch<T, simdette::neon>;
template <typename T>
struct batch<T, simdette::sve>;

// WebAssembly backend
template <typename T>
struct batch<T, simdette::simd128>;

// ============================================================================
// Type trait for getting vector width (number of elements)
// ============================================================================

namespace detail {

/**
 * @brief Trait to get the number of elements in a batch.
 * Specialized for each architecture.
 */
template <typename T, typename Arch>
struct batch_width;

/**
 * @brief Get the vector width as a compile-time constant.
 */
template <typename T, typename Arch>
inline constexpr std::size_t batch_width_v = batch_width<T, Arch>::value;

} // namespace detail

// ============================================================================
// Generic batch interface (to be specialized per architecture)
// ============================================================================

template <typename T, typename Arch>
struct batch {
    static constexpr std::size_t width = detail::batch_width_v<T, Arch>;

    /**
     * @brief Default constructor. Does not initialize values.
     */
    SIMDETTE_ALWAYS_INLINE batch() noexcept = default;

    /**
     * @brief Construct from a scalar value (broadcast).
     */
    SIMDETTE_ALWAYS_INLINE batch(T value) noexcept;

    /**
     * @brief Construct from an array of values.
     */
    SIMDETTE_ALWAYS_INLINE batch(T const* values) noexcept;

    /**
     * @brief Load values from memory with specified alignment.
     * @param ptr Pointer to memory (must be aligned to Simdette::Alignment).
     * @return batch loaded from memory
     */
    [[nodiscard]]
    static SIMDETTE_ALWAYS_INLINE batch load_aligned(T const* ptr) noexcept;

    /**
     * @brief Load values from memory without alignment requirement.
     * @param ptr Pointer to memory.
     * @return batch loaded from memory
     */
    [[nodiscard]]
    static SIMDETTE_ALWAYS_INLINE batch load_unaligned(T const* ptr) noexcept;

    /**
     * @brief Store values to memory with specified alignment.
     * @param ptr Destination pointer (must be aligned to Simdette::Alignment).
     */
    SIMDETTE_ALWAYS_INLINE void store_aligned(T* ptr) noexcept;

    /**
     * @brief Store values to memory without alignment requirement.
     * @param ptr Destination pointer.
     */
    SIMDETTE_ALWAYS_INLINE void store_unaligned(T* ptr) noexcept;

    /**
     * @brief Extract a scalar value at the given index.
     */
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE T extract(std::size_t index) const noexcept;

    /**
     * @brief Convert to array of scalar values.
     */
    SIMDETTE_ALWAYS_INLINE void to_array(T* ptr) const noexcept;

    // ========================================================================
    // Arithmetic operators
    // ========================================================================

    /**
     * @brief Element-wise addition.
     */
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator+(batch const& other) const noexcept;

    /**
     * @brief Element-wise subtraction.
     */
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator-(batch const& other) const noexcept;

    /**
     * @brief Element-wise multiplication.
     */
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator*(batch const& other) const noexcept;

    /**
     * @brief Element-wise division.
     */
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator/(batch const& other) const noexcept;

    /**
     * @brief Compound addition.
     */
    SIMDETTE_ALWAYS_INLINE batch& operator+=(batch const& other) noexcept;

    /**
     * @brief Compound subtraction.
     */
    SIMDETTE_ALWAYS_INLINE batch& operator-=(batch const& other) noexcept;

    /**
     * @brief Compound multiplication.
     */
    SIMDETTE_ALWAYS_INLINE batch& operator*=(batch const& other) noexcept;

    /**
     * @brief Compound division.
     */
    SIMDETTE_ALWAYS_INLINE batch& operator/=(batch const& other) noexcept;

    // ========================================================================
    // Comparison operators
    // ========================================================================

    /**
     * @brief Element-wise less-than comparison.
     * @return batch of boolean masks (all bits set for true, 0 for false)
     */
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator<(batch const& other) const noexcept;

    /**
     * @brief Element-wise less-than-or-equal comparison.
     */
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator<=(batch const& other) const noexcept;

    /**
     * @brief Element-wise greater-than comparison.
     */
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator>(batch const& other) const noexcept;

    /**
     * @brief Element-wise greater-than-or-equal comparison.
     */
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator>=(batch const& other) const noexcept;

    /**
     * @brief Element-wise equality comparison.
     */
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator==(batch const& other) const noexcept;

    /**
     * @brief Element-wise inequality comparison.
     */
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator!=(batch const& other) const noexcept;

    // ========================================================================
    // Bitwise operators (for integer types)
    // ========================================================================

    /**
     * @brief Element-wise bitwise AND.
     */
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator&(batch const& other) const noexcept;

    /**
     * @brief Element-wise bitwise OR.
     */
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator|(batch const& other) const noexcept;

    /**
     * @brief Element-wise bitwise XOR.
     */
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator^(batch const& other) const noexcept;

    /**
     * @brief Element-wise bitwise NOT (complement).
     */
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator~() const noexcept;

    // ========================================================================
    // Logical operators (for boolean types)
    // ========================================================================

    /**
     * @brief Element-wise logical AND.
     */
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator&&(batch const& other) const noexcept;

    /**
     * @brief Element-wise logical OR.
     */
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator||(batch const& other) const noexcept;

    /**
     * @brief Element-wise logical NOT.
     */
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator!() const noexcept;

    // ========================================================================
    // Unary operators
    // ========================================================================

    /**
     * @brief Unary negation.
     */
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator-() const noexcept;

    /**
     * @brief Unary plus (no-op).
     */
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator+() const noexcept;

    // ========================================================================
    // Horizontal operations (across vector lanes)
    // ========================================================================

    /**
     * @brief Horizontal sum of all elements.
     */
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE T reduce_add() const noexcept;

    /**
     * @brief Horizontal minimum of all elements.
     */
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE T reduce_min() const noexcept;

    /**
     * @brief Horizontal maximum of all elements.
     */
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE T reduce_max() const noexcept;

    /**
     * @brief Check if all elements are non-zero (true).
     */
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE bool all() const noexcept;

    /**
     * @brief Check if any element is non-zero (true).
     */
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE bool any() const noexcept;
};

// ============================================================================
// Non-member arithmetic operators (for better expression typing)
// ============================================================================

/**
 * @brief Scalar * batch
 */
template <typename T, typename Arch>
[[nodiscard]]
SIMDETTE_ALWAYS_INLINE batch<T, Arch> operator*(T scalar, batch<T, Arch> const& vec) noexcept;

/**
 * @brief batch * scalar
 */
template <typename T, typename Arch>
[[nodiscard]]
SIMDETTE_ALWAYS_INLINE batch<T, Arch> operator*(batch<T, Arch> const& vec, T scalar) noexcept;

/**
 * @brief Scalar / batch (element-wise)
 */
template <typename T, typename Arch>
[[nodiscard]]
SIMDETTE_ALWAYS_INLINE batch<T, Arch> operator/(T scalar, batch<T, Arch> const& vec) noexcept;

/**
 * @brief batch / scalar (element-wise)
 */
template <typename T, typename Arch>
[[nodiscard]]
SIMDETTE_ALWAYS_INLINE batch<T, Arch> operator/(batch<T, Arch> const& vec, T scalar) noexcept;

/**
 * @brief Scalar + batch
 */
template <typename T, typename Arch>
[[nodiscard]]
SIMDETTE_ALWAYS_INLINE batch<T, Arch> operator+(T scalar, batch<T, Arch> const& vec) noexcept;

/**
 * @brief batch + scalar
 */
template <typename T, typename Arch>
[[nodiscard]]
SIMDETTE_ALWAYS_INLINE batch<T, Arch> operator+(batch<T, Arch> const& vec, T scalar) noexcept;

/**
 * @brief Scalar - batch
 */
template <typename T, typename Arch>
[[nodiscard]]
SIMDETTE_ALWAYS_INLINE batch<T, Arch> operator-(T scalar, batch<T, Arch> const& vec) noexcept;

/**
 * @brief batch - scalar
 */
template <typename T, typename Arch>
[[nodiscard]]
SIMDETTE_ALWAYS_INLINE batch<T, Arch> operator-(batch<T, Arch> const& vec, T scalar) noexcept;

// ============================================================================
// Comparison result type traits
// ============================================================================

namespace detail {

/**
 * @brief Traits for conversion of result types for comparison operators.
 */
template <typename T>
struct comparison_result;

/**
 * @brief Float comparison returns same type (mask in bits).
 */
template <>
struct comparison_result<float> {
    using type = float;
};

/**
 * @brief Double comparison returns same type (mask in bits).
 */
template <>
struct comparison_result<double> {
    using type = double;
};

/**
 * @brief Integer comparison returns same type (mask in bits).
 */
template <typename T>
    requires std::is_integral_v<T>
struct comparison_result<T> {
    using type = T;
};

} // namespace detail

} // namespace simdette
