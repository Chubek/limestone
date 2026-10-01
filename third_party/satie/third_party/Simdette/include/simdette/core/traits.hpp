#pragma once

#include <type_traits>
#include <cstdint>
#include <limits>

namespace simdette {

// ============================================================================
// Type traits for SIMD compatibility
// ============================================================================

/**
 * @brief Trait to check if a type is SIMD-compatible (float, double, or integral).
 */
template <typename T>
struct is_simd_compatible : std::false_type {};

template <>
struct is_simd_compatible<float> : std::true_type {};

template <>
struct is_simd_compatible<double> : std::true_type {};

template <>
struct is_simd_compatible<int8_t> : std::true_type {};

template <>
struct is_simd_compatible<int16_t> : std::true_type {};

template <>
struct is_simd_compatible<int32_t> : std::true_type {};

template <>
struct is_simd_compatible<int64_t> : std::true_type {};

template <>
struct is_simd_compatible<uint8_t> : std::true_type {};

template <>
struct is_simd_compatible<uint16_t> : std::true_type {};

template <>
struct is_simd_compatible<uint32_t> : std::true_type {};

template <>
struct is_simd_compatible<uint64_t> : std::true_type {};

/**
 * @brief Check if a type is SIMD-compatible.
 */
template <typename T>
inline constexpr bool is_simd_compatible_v = is_simd_compatible<T>::value;

// ============================================================================
// Traits for floating-point types
// ============================================================================

/**
 * @brief Trait to check if a type is floating-point.
 */
template <typename T>
struct is_floating_point : std::is_floating_point<T> {};

template <typename T>
inline constexpr bool is_floating_point_v = is_floating_point<T>::value;

/**
 * @brief Get the floating-point value type (float, double, long double).
 * For integer types, defaults to float.
 */
template <typename T, typename Enable = void>
struct float_type {
    using type = float;
};

template <>
struct float_type<float> {
    using type = float;
};

template <>
struct float_type<double> {
    using type = double;
};

template <>
struct float_type<long double> {
    using type = long double;
};

/**
 * @brief Get the floating-point value type.
 */
template <typename T>
using float_type_t = typename float_type<T>::type;

// ============================================================================
// Traits for integral types
// ============================================================================

/**
 * @brief Trait to check if a type is integral.
 */
template <typename T>
struct is_integral : std::is_integral<T> {};

template <typename T>
inline constexpr bool is_integral_v = is_integral<T>::value;

/**
 * @brief Trait to check if a type is signed integral.
 */
template <typename T>
struct is_signed_integral : std::integral_constant<bool, 
    is_integral_v<T> && std::is_signed<T>::value> {};

template <typename T>
inline constexpr bool is_signed_integral_v = is_signed_integral<T>::value;

/**
 * @brief Trait to check if a type is unsigned integral.
 */
template <typename T>
struct is_unsigned_integral : std::integral_constant<bool, 
    is_integral_v<T> && std::is_unsigned<T>::value> {};

template <typename T>
inline constexpr bool is_unsigned_integral_v = is_unsigned_integral<T>::value;

/**
 * @brief Get the signed version of an integral type.
 */
template <typename T>
struct signed_type {
    using type = typename std::conditional<
        std::is_same<T, uint8_t>::value, int8_t,
        typename std::conditional<
            std::is_same<T, uint16_t>::value, int16_t,
            typename std::conditional<
                std::is_same<T, uint32_t>::value, int32_t,
                typename std::conditional<
                    std::is_same<T, uint64_t>::value, int64_t,
                    T
                >::type
            >::type
        >::type
    >::type;
};

/**
 * @brief Get the signed version of an integral type.
 */
template <typename T>
using signed_type_t = typename signed_type<T>::type;

/**
 * @brief Get the unsigned version of an integral type.
 */
template <typename T>
struct unsigned_type {
    using type = typename std::conditional<
        std::is_same<T, int8_t>::value, uint8_t,
        typename std::conditional<
            std::is_same<T, int16_t>::value, uint16_t,
            typename std::conditional<
                std::is_same<T, int32_t>::value, uint32_t,
                typename std::conditional<
                    std::is_same<T, int64_t>::value, uint64_t,
                    T
                >::type
            >::type
        >::type
    >::type;
};

/**
 * @brief Get the unsigned version of an integral type.
 */
template <typename T>
using unsigned_type_t = typename unsigned_type<T>::type;

// ============================================================================
// Traits for width detection
// ============================================================================

/**
 * @brief Get the bit width of a type.
 */
template <typename T>
struct bit_width : std::integral_constant<int, sizeof(T) * 8> {};

/**
 * @brief Get the bit width of a type.
 */
template <typename T>
inline constexpr int bit_width_v = bit_width<T>::value;

/**
 * @brief Get the element count for a given vector size and type.
 */
template <int VectorBytes, typename T>
struct element_count : std::integral_constant<int, VectorBytes / sizeof(T)> {};

template <int VectorBytes, typename T>
inline constexpr int element_count_v = element_count<VectorBytes, T>::value;

// ============================================================================
// Alignment traits
// ============================================================================

/**
 * @brief Get the recommended alignment for SIMD operations.
 */
template <typename T>
struct simd_alignment : std::integral_constant<std::size_t, alignof(T)> {};

// Specialize for SIMD types with larger alignment
template <>
struct simd_alignment<float> : std::integral_constant<std::size_t, 32> {};

template <>
struct simd_alignment<double> : std::integral_constant<std::size_t, 32> {};

template <>
struct simd_alignment<int32_t> : std::integral_constant<std::size_t, 32> {};

template <>
struct simd_alignment<int64_t> : std::integral_constant<std::size_t, 32> {};

template <>
struct simd_alignment<uint32_t> : std::integral_constant<std::size_t, 32> {};

template <>
struct simd_alignment<uint64_t> : std::integral_constant<std::size_t, 32> {};

/**
 * @brief Get the recommended alignment for SIMD operations.
 */
template <typename T>
inline constexpr std::size_t simd_alignment_v = simd_alignment<T>::value;

// ============================================================================
// Traits for arithmetic operations
// ============================================================================

/**
 * @brief Traits for determining if a type supports SIMD addition.
 */
template <typename T>
struct has_simd_add : std::true_type {};

/**
 * @brief Traits for determining if a type supports SIMD multiplication.
 */
template <typename T>
struct has_simd_mul : std::true_type {};

/**
 * @brief Traits for determining if a type supports SIMD comparison.
 */
template <typename T>
struct has_simd_compare : std::true_type {};

// ============================================================================
// Type aliases for common SIMD types
// ============================================================================

/**
 * @brief 32-bit floating-point type.
 */
using simd_float = float;

/**
 * @brief 64-bit floating-point type.
 */
using simd_double = double;

/**
 * @brief 32-bit signed integer type.
 */
using simd_int32 = int32_t;

/**
 * @brief 64-bit signed integer type.
 */
using simd_int64 = int64_t;

/**
 * @brief 32-bit unsigned integer type.
 */
using simd_uint32 = uint32_t;

/**
 * @brief 64-bit unsigned integer type.
 */
using simd_uint64 = uint64_t;

// ============================================================================
// Helper constexpr functions
// ============================================================================

/**
 * @brief Check if type T is suitable for SIMD operations.
 */
template <typename T>
[[nodiscard]]
constexpr bool is_simdable() noexcept {
    return is_simd_compatible_v<T>;
}

/**
 * @brief Get the number of elements that fit in a 256-bit vector for type T.
 */
template <typename T>
[[nodiscard]]
constexpr int vector_256_count() noexcept {
    static_assert(is_simd_compatible_v<T>, "Type must be SIMD-compatible");
    return 256 / (sizeof(T) * 8);
}

/**
 * @brief Get the number of elements that fit in a 512-bit vector for type T.
 */
template <typename T>
[[nodiscard]]
constexpr int vector_512_count() noexcept {
    static_assert(is_simd_compatible_v<T>, "Type must be SIMD-compatible");
    return 512 / (sizeof(T) * 8);
}

} // namespace simdette
